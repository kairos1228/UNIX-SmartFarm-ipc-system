/*
 * ==============================================================================
 * 파일명: main_server.c
 * 역할: [P3] 중앙 서버 프로세스 - Resource Manager & Logic Controller
 *
 * 기술 요소:
 *   - Message Queue: 센서 데이터 수신, 제어 명령 송신
 *   - Shared Memory: 설정값(임계값) 읽기
 *   - Semaphore: 공유 메모리 동기화
 *   - Signal Handler: 안전한 종료 및 IPC 자원 정리
 *
 * 작성자: Virtual SmartFarm Team
 * 작성일: 2025-12-02
 * ==============================================================================
 */

#include "../include/common.h"

static int msg_queue_id = -1;
static int control_queue_id = -1;
static int shm_id = -1;
static int sem_id = -1;
static SharedData *shared_data = NULL;

void cleanup_resources() {
    printf("\n[SERVER] IPC 자원 정리 중...\n");

    if (shared_data != NULL) {
        shmdt(shared_data);
    }

    if (msg_queue_id != -1) {
        msgctl(msg_queue_id, IPC_RMID, NULL);
    }

    if (control_queue_id != -1) {
        msgctl(control_queue_id, IPC_RMID, NULL);
    }

    if (shm_id != -1) {
        shmctl(shm_id, IPC_RMID, NULL);
    }

    if (sem_id != -1) {
        semctl(sem_id, 0, IPC_RMID);
    }

    printf("[SERVER] 자원 정리 완료\n");
}

void signal_handler(int signo) {
    cleanup_resources();
    exit(0);
}

int main() {
    printf("==================================================\n");
    printf("  가상 스마트팜 중앙 서버 [P3] 시작\n");
    printf("==================================================\n\n");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    msg_queue_id = msgget(MSG_KEY_DATA, 0666 | IPC_CREAT);
    control_queue_id = msgget(MSG_KEY_CONTROL, 0666 | IPC_CREAT);
    shm_id = shmget(SHM_KEY, sizeof(SharedData), 0666 | IPC_CREAT);
    sem_id = semget(SEM_KEY, 1, 0666 | IPC_CREAT);

    if (msg_queue_id == -1 || control_queue_id == -1 ||
        shm_id == -1 || sem_id == -1) {
        perror("[SERVER] IPC 자원 생성 실패");
        exit(1);
    }

    shared_data = (SharedData *)shmat(shm_id, NULL, 0);
    if (shared_data == (void *)-1) {
        perror("[SERVER] 공유 메모리 연결 실패");
        exit(1);
    }

    union semun sem_union;
    sem_union.val = 1;
    semctl(sem_id, 0, SETVAL, sem_union);

    shared_data->temp_threshold = 28;
    shared_data->humidity_threshold = 70;
    shared_data->system_running = 1;

    printf("[SERVER] 초기화 완료\n");
    printf("[SERVER] 온도 임계값: %d°C\n", shared_data->temp_threshold);
    printf("[SERVER] 습도 임계값: %d%%\n\n", shared_data->humidity_threshold);

    while (1) {
        SensorDataMsg sensor_msg;
        ssize_t result = msgrcv(msg_queue_id, &sensor_msg,
                               sizeof(SensorDataMsg) - sizeof(long),
                               MSG_TYPE_SENSOR_DATA, IPC_NOWAIT);

        if (result != -1) {
            sem_lock(sem_id);
            int temp_thresh = shared_data->temp_threshold;
            int hum_thresh = shared_data->humidity_threshold;
            sem_unlock(sem_id);

            printf("[SERVER] 센서 데이터 - 온도: %.2f°C, 습도: %.2f%%\n",
                   sensor_msg.temperature, sensor_msg.humidity);

            ControlCmdMsg cmd_msg;
            cmd_msg.msg_type = MSG_TYPE_CONTROL_CMD;
            cmd_msg.heater_on = (sensor_msg.temperature < temp_thresh) ? 1 : 0;
            cmd_msg.fan_on = (sensor_msg.humidity > hum_thresh) ? 1 : 0;
            cmd_msg.led_on = 1;

            msgsnd(control_queue_id, &cmd_msg,
                   sizeof(ControlCmdMsg) - sizeof(long), 0);
        }

        sleep(1);
    }

    cleanup_resources();
    return 0;
}
