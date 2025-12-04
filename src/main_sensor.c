/*
 * ==============================================================================
 * 파일명: main_sensor.c
 * 역할: [P1] 가상 센서 프로세스 - Virtual Physics Engine 탑재
 *
 * 기술 요소:
 *   - 가상 물리 엔진: 단순 rand() 대신 실제 물리 법칙 시뮬레이션
 *   - 온도: 히터 상태에 따라 동적으로 상승/하강
 *   - 습도: 팬 상태에 따라 자연스럽게 변화
 *   - Message Queue: 센서 데이터를 서버로 전송
 *   - Shared Memory: 제어 상태(히터/팬) 읽기
 *
 * 물리 모델:
 *   - 히터 ON: 온도가 0.2도/초 상승
 *   - 히터 OFF: 주변 온도(25도)로 0.05도/초 하강 (뉴턴 냉각 법칙)
 *   - 팬 ON: 습도가 0.5%/초 감소
 *   - 팬 OFF: 습도가 0.3%/초 증가
 *
 * [변경사항] 제어 상태를 Shared Memory에서 읽기
 *           → 메시지 큐 경쟁 문제 해결
 *
 * 작성자: Virtual SmartFarm Team
 * 작성일: 2025-12-02
 * ==============================================================================
 */

#include "../include/common.h"

/* ============================================================================
 * 전역 변수 - 가상 물리 상태
 * ============================================================================ */
static float current_temp = 25.0;      // 현재 온도 (초기값 25도)
static float current_humidity = 50.0;  // 현재 습도 (초기값 50%)
static int heater_state = 0;           // 히터 상태 (0=OFF, 1=ON)
static int fan_state = 0;              // 팬 상태 (0=OFF, 1=ON)

/* IPC 자원 */
static int msg_queue_id = -1;          // 메시지 큐 ID (데이터 전송용)
static int shm_id = -1;                // 공유 메모리 ID
static int sem_id = -1;                // 세마포어 ID
static SharedData *shared_data = NULL; // 공유 메모리 포인터

/* ============================================================================
 * 함수: cleanup_and_exit
 * 설명: 시그널 핸들러 - 프로세스 종료 시 자원 정리
 * ============================================================================ */
void cleanup_and_exit(int signo) {
    (void)signo;  // unused parameter 경고 방지
    printf("\n[SENSOR] 종료 신호 수신. 프로세스 종료 중...\n");
    if (shared_data != NULL) {
        shmdt(shared_data);
    }
    exit(0);
}

/* ============================================================================
 * 함수: update_physics
 * 설명: 가상 물리 엔진 - 온도와 습도를 시뮬레이션
 *
 * 물리 법칙:
 *   1. 온도 변화
 *      - 히터 ON: temp += 0.2 (가열)
 *      - 히터 OFF: temp -= (temp - 25.0) * 0.05 (자연 냉각)
 *
 *   2. 습도 변화
 *      - 팬 ON: humidity -= 0.5 (환기)
 *      - 팬 OFF: humidity += 0.3 (수분 증가)
 * ============================================================================ */
void update_physics() {
    // 온도 물리 시뮬레이션
    if (heater_state == 1) {
        // 히터가 켜져 있으면 온도 상승
        current_temp += 0.2;
        // 최대 온도 제한 (40도)
        if (current_temp > 40.0) {
            current_temp = 40.0;
        }
    } else {
        // 히터가 꺼져 있으면 주변 온도(25도)로 서서히 하강
        // 뉴턴의 냉각 법칙: dT/dt = -k(T - T_ambient)
        current_temp -= (current_temp - 25.0) * 0.05;
        // 최저 온도 제한 (20도)
        if (current_temp < 20.0) {
            current_temp = 20.0;
        }
    }

    // 습도 물리 시뮬레이션
    if (fan_state == 1) {
        // 팬이 켜져 있으면 습도 감소 (환기 효과)
        current_humidity -= 0.5;
        // 최저 습도 제한 (30%)
        if (current_humidity < 30.0) {
            current_humidity = 30.0;
        }
    } else {
        // 팬이 꺼져 있으면 습도 증가 (자연 증발)
        current_humidity += 0.3;
        // 최대 습도 제한 (90%)
        if (current_humidity > 90.0) {
            current_humidity = 90.0;
        }
    }

    // 실제 환경을 모사하기 위한 미세 노이즈 추가 (±0.1 범위)
    current_temp += ((float)(rand() % 21) - 10.0) / 100.0;
    current_humidity += ((float)(rand() % 21) - 10.0) / 100.0;
}

/* ============================================================================
 * 함수: read_control_state
 * 설명: 공유 메모리에서 제어 상태(히터/팬) 읽기
 *       - 세마포어로 동기화하여 Race Condition 방지
 * ============================================================================ */
void read_control_state() {
    sem_lock(sem_id);
    int prev_heater = heater_state;
    int prev_fan = fan_state;
    heater_state = shared_data->heater_on;
    fan_state = shared_data->fan_on;
    sem_unlock(sem_id);

    // 상태 변경 시에만 출력
    if (prev_heater != heater_state || prev_fan != fan_state) {
        printf("[SENSOR] 제어 상태 변경 - 히터:%s, 팬:%s\n",
               heater_state ? "ON" : "OFF",
               fan_state ? "ON" : "OFF");
    }
}

/* ============================================================================
 * 함수: send_sensor_data
 * 설명: 센서 데이터를 메시지 큐를 통해 서버로 전송
 * ============================================================================ */
void send_sensor_data() {
    SensorDataMsg sensor_msg;

    // 메시지 구조체 초기화
    sensor_msg.msg_type = MSG_TYPE_SENSOR_DATA;
    sensor_msg.temperature = current_temp;
    sensor_msg.humidity = current_humidity;
    sensor_msg.timestamp = time(NULL);

    // msgsnd: 메시지 큐에 센서 데이터 전송
    if (msgsnd(msg_queue_id, &sensor_msg,
               sizeof(SensorDataMsg) - sizeof(long), 0) == -1) {
        perror("[SENSOR] 데이터 전송 실패");
    } else {
        printf("[SENSOR] 데이터 전송 - 온도: %.2f°C, 습도: %.2f%%\n",
               current_temp, current_humidity);
    }
}

/* ============================================================================
 * 메인 함수
 * ============================================================================ */
int main() {
    printf("==================================================\n");
    printf("  가상 스마트팜 센서 프로세스 [P1] 시작\n");
    printf("  - 가상 물리 엔진 탑재\n");
    printf("  - Message Queue: 데이터 전송\n");
    printf("  - Shared Memory: 제어 상태 읽기\n");
    printf("==================================================\n");
    printf("  PID: %d\n\n", getpid());

    // 시그널 핸들러 등록 (Ctrl+C 처리)
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);

    // 난수 생성기 초기화 (미세 노이즈용)
    srand(time(NULL));

    // ========================================================================
    // IPC 자원 연결
    // ========================================================================

    // 메시지 큐 연결 (센서 데이터 전송용)
    msg_queue_id = msgget(MSG_KEY_DATA, 0666);
    if (msg_queue_id == -1) {
        perror("[SENSOR] 메시지 큐 연결 실패 (서버를 먼저 실행하세요)");
        exit(1);
    }
    printf("[SENSOR] 메시지 큐 연결 성공 (ID: %d)\n", msg_queue_id);

    // 공유 메모리 연결
    shm_id = shmget(SHM_KEY, sizeof(SharedData), 0666);
    if (shm_id == -1) {
        perror("[SENSOR] 공유 메모리 연결 실패 (서버를 먼저 실행하세요)");
        exit(1);
    }
    shared_data = (SharedData *)shmat(shm_id, NULL, 0);
    if (shared_data == (void *)-1) {
        perror("[SENSOR] 공유 메모리 attach 실패");
        exit(1);
    }
    printf("[SENSOR] 공유 메모리 연결 성공 (ID: %d)\n", shm_id);

    // 세마포어 연결
    sem_id = semget(SEM_KEY, 1, 0666);
    if (sem_id == -1) {
        perror("[SENSOR] 세마포어 연결 실패 (서버를 먼저 실행하세요)");
        exit(1);
    }
    printf("[SENSOR] 세마포어 연결 성공 (ID: %d)\n\n", sem_id);

    // ========================================================================
    // 메인 루프: 0.5초마다 물리 시뮬레이션 및 1초마다 데이터 전송
    // ========================================================================
    int loop_count = 0;
    while (1) {
        // 시스템 종료 확인
        sem_lock(sem_id);
        int running = shared_data->system_running;
        sem_unlock(sem_id);
        if (!running) {
            printf("[SENSOR] 서버 종료 신호 수신. 프로세스 종료.\n");
            break;
        }

        // 공유 메모리에서 제어 상태 읽기 (히터/팬)
        read_control_state();

        // 가상 물리 엔진 실행 (온도/습도 업데이트)
        update_physics();

        // 1초마다 센서 데이터 전송 (0.5초 * 2회)
        if (loop_count % 2 == 0) {
            send_sensor_data();
        }

        loop_count++;
        usleep(500000);  // 0.5초 대기 (500,000 마이크로초)
    }

    return 0;
}
