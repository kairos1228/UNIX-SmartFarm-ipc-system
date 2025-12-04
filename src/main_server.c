/*
 * ==============================================================================
 * 파일명: main_server.c
 * 역할: [P3] 중앙 서버 프로세스 - Resource Manager & Logic Controller
 *
 * 기술 요소:
 *   - Message Queue: 센서 데이터 수신
 *   - Shared Memory: 설정값(임계값) 읽기, 제어 상태 쓰기
 *   - Semaphore: 공유 메모리 동기화
 *   - Signal Handler: 안전한 종료 및 IPC 자원 정리
 *   - fork(): 로그 기록 전용 자식 프로세스 생성
 *   - pipe(): 부모-자식 간 로그 데이터 전송
 *   - pthread: 센서 데이터 처리 스레드
 *   - getpid(), getppid(): 프로세스 정보 조회
 *
 * 프로세스 구조:
 *   [부모 프로세스] - 센서 데이터 수신, 제어 로직
 *        │
 *        ├── pipe ──→ [자식 프로세스] - 로그 파일 기록
 *        │
 *        └── pthread ──→ [스레드] - 경고 모니터링
 *
 * 작성자: Virtual SmartFarm Team
 * 작성일: 2025-12-02
 * ==============================================================================
 */

#include "../include/common.h"

/* ============================================================================
 * 전역 변수
 * ============================================================================ */
static int msg_queue_id = -1;
static int shm_id = -1;
static int sem_id = -1;
static SharedData *shared_data = NULL;

/* fork & pipe 관련 */
static pid_t logger_pid = -1;       // 로그 기록 자식 프로세스 PID
static int pipe_fd[2] = {-1, -1};   // 파이프: [0]=읽기, [1]=쓰기

/* pthread 관련 */
static pthread_t alert_thread;
static int thread_running = 1;

/* ============================================================================
 * 로그 메시지 구조체 (파이프 전송용)
 * ============================================================================ */
typedef struct {
    float temperature;
    float humidity;
    int heater_on;
    int fan_on;
    time_t timestamp;
} LogMessage;

/* ============================================================================
 * 함수: logger_process
 * 설명: 자식 프로세스 - 파이프에서 데이터를 읽어 로그 파일에 기록
 *       fork()로 생성된 별도 프로세스에서 실행
 * ============================================================================ */
void logger_process() {
    printf("[LOGGER:%d] 로그 기록 프로세스 시작 (부모 PID: %d)\n", 
           getpid(), getppid());
    
    // 쓰기 끝 닫기 (자식은 읽기만)
    close(pipe_fd[1]);
    
    // 로그 파일 열기
    FILE *log_file = fopen("smartfarm.log", "a");
    if (log_file == NULL) {
        perror("[LOGGER] 로그 파일 열기 실패");
        exit(1);
    }
    
    // 로그 헤더 기록
    time_t now = time(NULL);
    fprintf(log_file, "\n========== 로그 시작: %s", ctime(&now));
    fprintf(log_file, "%-20s  %8s  %8s  %6s  %4s\n", 
            "시간", "온도(°C)", "습도(%)", "히터", "팬");
    fprintf(log_file, "----------------------------------------------------\n");
    fflush(log_file);
    
    // 파이프에서 데이터 읽기 루프
    LogMessage log_msg;
    while (read(pipe_fd[0], &log_msg, sizeof(LogMessage)) > 0) {
        struct tm *t = localtime(&log_msg.timestamp);
        fprintf(log_file, "%04d-%02d-%02d %02d:%02d:%02d  %8.2f  %8.2f  %6s  %4s\n",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec,
                log_msg.temperature, log_msg.humidity,
                log_msg.heater_on ? "ON" : "OFF",
                log_msg.fan_on ? "ON" : "OFF");
        fflush(log_file);
    }
    
    // 종료 처리
    fprintf(log_file, "========== 로그 종료: %s", ctime(&(time_t){time(NULL)}));
    fclose(log_file);
    close(pipe_fd[0]);
    
    printf("[LOGGER:%d] 로그 기록 프로세스 종료\n", getpid());
    exit(0);
}

/* ============================================================================
 * 함수: alert_thread_func
 * 설명: 경고 모니터링 스레드 - 임계값 초과 시 경고 출력
 *       pthread로 생성된 별도 스레드에서 실행
 * ============================================================================ */
void *alert_thread_func(void *arg) {
    (void)arg;
    printf("[THREAD:0x%lx] 경고 모니터링 스레드 시작\n", pthread_self());
    
    while (thread_running) {
        sem_lock(sem_id);
        float temp = shared_data->current_temp;
        float hum = shared_data->current_humidity;
        int temp_thresh = shared_data->temp_threshold;
        int hum_thresh = shared_data->humidity_threshold;
        sem_unlock(sem_id);
        
        // 경고 조건 체크
        if (temp > temp_thresh + 5) {
            printf("\a[ALERT] ⚠️  고온 경고! 현재 온도: %.1f°C (임계값+5 초과)\n", temp);
        }
        if (temp < 20.0) {
            printf("\a[ALERT] ⚠️  저온 경고! 현재 온도: %.1f°C (20°C 미만)\n", temp);
        }
        if (hum > hum_thresh + 10) {
            printf("\a[ALERT] ⚠️  고습 경고! 현재 습도: %.1f%% (임계값+10 초과)\n", hum);
        }
        
        sleep(3);  // 3초마다 체크
    }
    
    printf("[THREAD] 경고 모니터링 스레드 종료\n");
    return NULL;
}

/* ============================================================================
 * 함수: cleanup_resources
 * 설명: IPC 자원 정리 (프로그램 종료 시 호출)
 * ============================================================================ */
void cleanup_resources() {
    printf("\n[SERVER:%d] 시스템 종료 시작...\n", getpid());

    // 1. 스레드 종료
    thread_running = 0;
    pthread_join(alert_thread, NULL);
    printf("[SERVER] 경고 스레드 종료 완료\n");

    // 2. 다른 프로세스들에게 종료 신호 전송
    if (shared_data != NULL) {
        sem_lock(sem_id);
        shared_data->system_running = 0;
        sem_unlock(sem_id);
        printf("[SERVER] 종료 신호 전송 완료\n");
        sleep(1);
    }

    // 3. 파이프 닫기 (자식 프로세스 종료 유도)
    if (pipe_fd[1] != -1) {
        close(pipe_fd[1]);
        printf("[SERVER] 파이프 닫기 완료\n");
    }

    // 4. 자식 프로세스 종료 대기
    if (logger_pid > 0) {
        int status;
        waitpid(logger_pid, &status, 0);
        printf("[SERVER] 로그 프로세스(PID:%d) 종료 완료\n", logger_pid);
    }

    // 5. 공유 메모리 분리
    if (shared_data != NULL) {
        shmdt(shared_data);
    }

    // 6. IPC 자원 삭제
    if (msg_queue_id != -1) {
        msgctl(msg_queue_id, IPC_RMID, NULL);
        printf("[SERVER] 메시지 큐 삭제 완료\n");
    }
    if (shm_id != -1) {
        shmctl(shm_id, IPC_RMID, NULL);
        printf("[SERVER] 공유 메모리 삭제 완료\n");
    }
    if (sem_id != -1) {
        semctl(sem_id, 0, IPC_RMID);
        printf("[SERVER] 세마포어 삭제 완료\n");
    }

    printf("[SERVER] 모든 자원 정리 완료\n");
}

/* ============================================================================
 * 함수: signal_handler
 * 설명: SIGINT/SIGTERM 처리 - 안전한 종료
 * ============================================================================ */
void signal_handler(int signo) {
    (void)signo;
    cleanup_resources();
    exit(0);
}

/* ============================================================================
 * 함수: print_system_info
 * 설명: 시스템 정보 출력 (uname 사용)
 * ============================================================================ */
void print_system_info() {
    struct utsname sys_info;
    if (uname(&sys_info) == 0) {
        printf("\n[시스템 정보]\n");
        printf("  OS: %s %s\n", sys_info.sysname, sys_info.release);
        printf("  호스트: %s\n", sys_info.nodename);
        printf("  아키텍처: %s\n", sys_info.machine);
    }
    printf("  서버 PID: %d\n", getpid());
    printf("\n");
}

/* ============================================================================
 * 메인 함수
 * ============================================================================ */
int main() {
    printf("==================================================\n");
    printf("  가상 스마트팜 중앙 서버 [P3] 시작\n");
    printf("==================================================\n");

    // 시스템 정보 출력
    print_system_info();

    // 시그널 핸들러 등록
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // ========================================================================
    // IPC 자원 생성
    // ========================================================================
    printf("[SERVER] IPC 자원 생성 중...\n");

    msg_queue_id = msgget(MSG_KEY_DATA, 0666 | IPC_CREAT);
    if (msg_queue_id == -1) {
        perror("[SERVER] 메시지 큐 생성 실패");
        exit(1);
    }
    printf("[SERVER] 메시지 큐 생성 완료 (ID: %d)\n", msg_queue_id);

    shm_id = shmget(SHM_KEY, sizeof(SharedData), 0666 | IPC_CREAT);
    if (shm_id == -1) {
        perror("[SERVER] 공유 메모리 생성 실패");
        exit(1);
    }
    printf("[SERVER] 공유 메모리 생성 완료 (ID: %d)\n", shm_id);

    sem_id = semget(SEM_KEY, 1, 0666 | IPC_CREAT);
    if (sem_id == -1) {
        perror("[SERVER] 세마포어 생성 실패");
        exit(1);
    }
    printf("[SERVER] 세마포어 생성 완료 (ID: %d)\n", sem_id);

    shared_data = (SharedData *)shmat(shm_id, NULL, 0);
    if (shared_data == (void *)-1) {
        perror("[SERVER] 공유 메모리 연결 실패");
        exit(1);
    }

    // 세마포어 초기화
    union semun sem_union;
    sem_union.val = 1;
    semctl(sem_id, 0, SETVAL, sem_union);

    // 공유 메모리 초기값 설정
    sem_lock(sem_id);
    shared_data->temp_threshold = 28;
    shared_data->humidity_threshold = 70;
    shared_data->heater_on = 0;
    shared_data->fan_on = 0;
    shared_data->led_on = 1;
    shared_data->current_temp = 25.0;
    shared_data->current_humidity = 50.0;
    shared_data->system_running = 1;
    sem_unlock(sem_id);

    printf("[SERVER] 초기 설정 - 온도 임계값: %d°C, 습도 임계값: %d%%\n",
           shared_data->temp_threshold, shared_data->humidity_threshold);

    // ========================================================================
    // 파이프 생성 및 fork() - 로그 기록 자식 프로세스
    // ========================================================================
    printf("\n[SERVER] 로그 프로세스 생성 중...\n");

    if (pipe(pipe_fd) == -1) {
        perror("[SERVER] 파이프 생성 실패");
        exit(1);
    }
    printf("[SERVER] 파이프 생성 완료 (읽기:%d, 쓰기:%d)\n", pipe_fd[0], pipe_fd[1]);

    logger_pid = fork();
    if (logger_pid == -1) {
        perror("[SERVER] fork 실패");
        exit(1);
    } else if (logger_pid == 0) {
        // 자식 프로세스: 로그 기록 담당
        logger_process();
        // logger_process()는 exit()으로 종료하므로 여기 도달 안 함
    }

    // 부모 프로세스: 읽기 끝 닫기
    close(pipe_fd[0]);
    printf("[SERVER] 로그 프로세스 생성 완료 (PID: %d)\n", logger_pid);

    // ========================================================================
    // pthread 생성 - 경고 모니터링 스레드
    // ========================================================================
    printf("\n[SERVER] 경고 모니터링 스레드 생성 중...\n");

    if (pthread_create(&alert_thread, NULL, alert_thread_func, NULL) != 0) {
        perror("[SERVER] 스레드 생성 실패");
        exit(1);
    }
    printf("[SERVER] 경고 스레드 생성 완료\n");

    // ========================================================================
    // 메인 루프: 센서 데이터 수신 → 제어 판단 → 파이프로 로그 전송
    // ========================================================================
    printf("\n[SERVER] 메인 루프 시작 (Ctrl+C로 종료)\n");
    printf("==================================================\n\n");

    while (1) {
        SensorDataMsg sensor_msg;

        ssize_t result = msgrcv(msg_queue_id, &sensor_msg,
                               sizeof(SensorDataMsg) - sizeof(long),
                               MSG_TYPE_SENSOR_DATA, IPC_NOWAIT);

        if (result != -1) {
            // 임계값 읽기
            sem_lock(sem_id);
            int temp_thresh = shared_data->temp_threshold;
            int hum_thresh = shared_data->humidity_threshold;
            sem_unlock(sem_id);

            printf("[SERVER] 센서 데이터 - 온도: %.2f°C, 습도: %.2f%%\n",
                   sensor_msg.temperature, sensor_msg.humidity);

            // 제어 로직
            int new_heater = (sensor_msg.temperature < temp_thresh) ? 1 : 0;
            int new_fan = (sensor_msg.humidity > hum_thresh) ? 1 : 0;

            // 공유 메모리에 상태 기록
            sem_lock(sem_id);
            shared_data->heater_on = new_heater;
            shared_data->fan_on = new_fan;
            shared_data->led_on = 1;
            shared_data->current_temp = sensor_msg.temperature;
            shared_data->current_humidity = sensor_msg.humidity;
            sem_unlock(sem_id);

            printf("[SERVER] 제어 명령 - 히터:%s, 팬:%s\n",
                   new_heater ? "ON" : "OFF",
                   new_fan ? "ON" : "OFF");

            // 파이프로 로그 데이터 전송 (자식 프로세스에게)
            LogMessage log_msg;
            log_msg.temperature = sensor_msg.temperature;
            log_msg.humidity = sensor_msg.humidity;
            log_msg.heater_on = new_heater;
            log_msg.fan_on = new_fan;
            log_msg.timestamp = time(NULL);
            write(pipe_fd[1], &log_msg, sizeof(LogMessage));
        }

        sleep(1);
    }

    cleanup_resources();
    return 0;
}
