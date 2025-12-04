/*
 * ==============================================================================
 * 파일명: main_actuator.c
 * 역할: [P2] 액추에이터 제어 프로세스 - ANSI UI Dashboard
 *
 * 기술 요소:
 *   - ANSI Escape Code를 사용한 컬러 터미널 UI
 *   - Shared Memory: 제어 상태(히터/팬/LED) 읽기
 *   - Semaphore: 동기화
 *   - 실시간 상태 표시 대시보드
 *
 * [변경사항] 제어 상태를 Shared Memory에서 읽기
 *           → 메시지 큐 경쟁 문제 해결
 *
 * 작성자: Virtual SmartFarm Team
 * 작성일: 2025-12-02
 * ==============================================================================
 */

#include "../include/common.h"

/* ANSI Color Codes */
#define ANSI_RESET   "\x1b[0m"
#define ANSI_RED     "\x1b[31m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_BLUE    "\x1b[34m"
#define ANSI_MAGENTA "\x1b[35m"
#define ANSI_CYAN    "\x1b[36m"
#define ANSI_BOLD    "\x1b[1m"

/* IPC 자원 */
static int shm_id = -1;
static int sem_id = -1;
static SharedData *shared_data = NULL;

/* 현재 상태 */
static int heater_on = 0;
static int fan_on = 0;
static int led_on = 0;

/* 센서 데이터 (Shared Memory에서 읽기) */
static float current_temp = 0.0;
static float current_humidity = 0.0;

/* ============================================================================
 * 함수: cleanup_and_exit
 * 설명: 시그널 핸들러 - 프로세스 종료 시 자원 정리
 * ============================================================================ */
void cleanup_and_exit(int signo) {
    (void)signo;  // unused parameter 경고 방지
    printf("\n[ACTUATOR] 종료 중...\n");
    if (shared_data != NULL) {
        shmdt(shared_data);
    }
    exit(0);
}

/* ============================================================================
 * 함수: display_dashboard
 * 설명: ANSI Escape Code를 사용한 컬러풀한 대시보드 출력
 * ============================================================================ */
void display_dashboard() {
    // 화면 클리어 (ANSI escape 사용 - system() 대신)
    printf("\033[2J\033[H");
    
    // 임계값 읽기
    sem_lock(sem_id);
    int temp_thresh = shared_data->temp_threshold;
    int hum_thresh = shared_data->humidity_threshold;
    sem_unlock(sem_id);
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║     " ANSI_BOLD ANSI_CYAN "🌱 SMART FARM ACTUATOR DASHBOARD 🌱" ANSI_RESET "        ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║                                                            ║\n");
    printf("║  📊 " ANSI_BOLD "현재 환경" ANSI_RESET "                                        ║\n");
    printf("║     🌡️  온도: %s%6.1f°C%s  (임계값: %d°C)                ║\n",
           current_temp >= temp_thresh ? ANSI_RED : ANSI_GREEN,
           current_temp, ANSI_RESET, temp_thresh);
    printf("║     💧 습도: %s%6.1f%% %s  (임계값: %d%%)                 ║\n",
           current_humidity > hum_thresh ? ANSI_RED : ANSI_GREEN,
           current_humidity, ANSI_RESET, hum_thresh);
    printf("║                                                            ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║  ⚙️  " ANSI_BOLD "장치 상태" ANSI_RESET "                                        ║\n");
    printf("║     🔥 히터(Heater):  %s%-6s%s                          ║\n",
           heater_on ? ANSI_RED : ANSI_BLUE,
           heater_on ? "[ON]" : "[OFF]",
           ANSI_RESET);
    printf("║     💨 팬(Fan):       %s%-6s%s                          ║\n",
           fan_on ? ANSI_GREEN : ANSI_BLUE,
           fan_on ? "[ON]" : "[OFF]",
           ANSI_RESET);
    printf("║     💡 LED:          %s%-6s%s                          ║\n",
           led_on ? ANSI_YELLOW : ANSI_BLUE,
           led_on ? "[ON]" : "[OFF]",
           ANSI_RESET);
    printf("║                                                            ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

/* ============================================================================
 * 함수: read_control_state
 * 설명: 공유 메모리에서 제어 상태 읽기 (세마포어 동기화)
 * ============================================================================ */
void read_control_state() {
    sem_lock(sem_id);
    heater_on = shared_data->heater_on;
    fan_on = shared_data->fan_on;
    led_on = shared_data->led_on;
    current_temp = shared_data->current_temp;
    current_humidity = shared_data->current_humidity;
    sem_unlock(sem_id);
}

/* ============================================================================
 * 메인 함수
 * ============================================================================ */
int main() {
    printf("[ACTUATOR] 프로세스 시작 (PID: %d)\n", getpid());

    // 시그널 핸들러 등록
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);

    // ========================================================================
    // IPC 자원 연결
    // ========================================================================

    // 공유 메모리 연결
    shm_id = shmget(SHM_KEY, sizeof(SharedData), 0666);
    if (shm_id == -1) {
        perror("[ACTUATOR] 공유 메모리 연결 실패 (서버를 먼저 실행하세요)");
        exit(1);
    }
    shared_data = (SharedData *)shmat(shm_id, NULL, 0);
    if (shared_data == (void *)-1) {
        perror("[ACTUATOR] 공유 메모리 attach 실패");
        exit(1);
    }
    printf("[ACTUATOR] 공유 메모리 연결 성공 (ID: %d)\n", shm_id);

    // 세마포어 연결
    sem_id = semget(SEM_KEY, 1, 0666);
    if (sem_id == -1) {
        perror("[ACTUATOR] 세마포어 연결 실패 (서버를 먼저 실행하세요)");
        exit(1);
    }
    printf("[ACTUATOR] 세마포어 연결 성공 (ID: %d)\n", sem_id);

    // ========================================================================
    // 메인 루프: 1초마다 상태 읽고 대시보드 갱신
    // ========================================================================
    while (1) {
        // 시스템 종료 확인
        sem_lock(sem_id);
        int running = shared_data->system_running;
        sem_unlock(sem_id);
        if (!running) {
            printf("\033[2J\033[H");  // 화면 클리어
            printf("[ACTUATOR] 서버 종료 신호 수신. 프로세스 종료.\n");
            break;
        }

        // 공유 메모리에서 제어 상태 읽기
        read_control_state();

        // 대시보드 표시
        display_dashboard();

        sleep(1);
    }

    return 0;
}
