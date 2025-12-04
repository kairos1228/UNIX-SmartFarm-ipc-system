/*
 * ==============================================================================
 * 파일명: main_actuator.c
 * 역할: [P2] 액추에이터 제어 프로세스 - ANSI UI Dashboard + ASCII 애니메이션
 *
 * 기술 요소:
 *   - ANSI Escape Code를 사용한 컬러 터미널 UI
 *   - ASCII Art 애니메이션 (히터 불꽃, 팬 회전, LED 깜빡임)
 *   - Shared Memory: 제어 상태(히터/팬/LED) 읽기
 *   - Semaphore: 동기화
 *   - 실시간 상태 표시 대시보드
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
#define ANSI_WHITE   "\x1b[37m"
#define ANSI_BOLD    "\x1b[1m"
#define ANSI_DIM     "\x1b[2m"
#define ANSI_ORANGE  "\x1b[38;5;208m"
#define ANSI_GRAY    "\x1b[38;5;240m"

/* IPC 자원 */
static int shm_id = -1;
static int sem_id = -1;
static SharedData *shared_data = NULL;

/* 현재 상태 */
static int heater_on = 0;
static int fan_on = 0;
static int led_on = 0;

/* 센서 데이터 */
static float current_temp = 0.0;
static float current_humidity = 0.0;

/* 애니메이션 프레임 카운터 */
static int frame = 0;

/* ============================================================================
 * 함수: cleanup_and_exit
 * ============================================================================ */
void cleanup_and_exit(int signo) {
    (void)signo;
    printf("\n[ACTUATOR] 종료 중...\n");
    if (shared_data != NULL) {
        shmdt(shared_data);
    }
    exit(0);
}

/* ============================================================================
 * 함수: display_dashboard
 * 설명: ASCII 애니메이션이 포함된 대시보드
 * ============================================================================ */
void display_dashboard() {
    // 화면 클리어
    printf("\033[2J\033[H");
    
    // 임계값 읽기
    sem_lock(sem_id);
    int temp_thresh = shared_data->temp_threshold;
    int hum_thresh = shared_data->humidity_threshold;
    sem_unlock(sem_id);
    
    printf("\n");
    printf("%s╔══════════════════════════════════════════════════════════════════════════╗%s\n", ANSI_CYAN, ANSI_RESET);
    printf("%s║%s       %s🌱 SMART FARM ACTUATOR DASHBOARD 🌱%s                              %s║%s\n", 
           ANSI_CYAN, ANSI_RESET, ANSI_BOLD ANSI_GREEN, ANSI_RESET, ANSI_CYAN, ANSI_RESET);
    printf("%s╠══════════════════════════════════════════════════════════════════════════╣%s\n", ANSI_CYAN, ANSI_RESET);
    printf("%s║%s                                                                          %s║%s\n", ANSI_CYAN, ANSI_RESET, ANSI_CYAN, ANSI_RESET);
    
    // 환경 정보
    printf("%s║%s  📊 %s현재 환경%s                                                           %s║%s\n",
           ANSI_CYAN, ANSI_RESET, ANSI_BOLD, ANSI_RESET, ANSI_CYAN, ANSI_RESET);
    printf("%s║%s     🌡️  온도: %s%6.1f°C%s  (임계값: %2d°C)  %s%s%s                           %s║%s\n",
           ANSI_CYAN, ANSI_RESET,
           current_temp >= temp_thresh ? ANSI_RED ANSI_BOLD : ANSI_GREEN,
           current_temp, ANSI_RESET, temp_thresh,
           current_temp >= temp_thresh ? ANSI_RED "▲ 고온!" : ANSI_GREEN "정상  ",
           ANSI_RESET, "", ANSI_CYAN, ANSI_RESET);
    printf("%s║%s     💧 습도: %s%6.1f%% %s  (임계값: %2d%%)   %s%s%s                           %s║%s\n",
           ANSI_CYAN, ANSI_RESET,
           current_humidity > hum_thresh ? ANSI_RED ANSI_BOLD : ANSI_GREEN,
           current_humidity, ANSI_RESET, hum_thresh,
           current_humidity > hum_thresh ? ANSI_RED "▲ 고습!" : ANSI_GREEN "정상  ",
           ANSI_RESET, "", ANSI_CYAN, ANSI_RESET);
    printf("%s║%s                                                                          %s║%s\n", ANSI_CYAN, ANSI_RESET, ANSI_CYAN, ANSI_RESET);
    printf("%s╠══════════════════════════════════════════════════════════════════════════╣%s\n", ANSI_CYAN, ANSI_RESET);
    printf("%s║%s  ⚙️  %s장치 상태%s                                                           %s║%s\n",
           ANSI_CYAN, ANSI_RESET, ANSI_BOLD, ANSI_RESET, ANSI_CYAN, ANSI_RESET);
    printf("%s║%s                                                                          %s║%s\n", ANSI_CYAN, ANSI_RESET, ANSI_CYAN, ANSI_RESET);
    
    // 장치 이름과 상태
    printf("%s║%s      🔥 HEATER           💨 FAN              💡 LED                %s║%s\n",
           ANSI_CYAN, ANSI_RESET, ANSI_CYAN, ANSI_RESET);
    printf("%s║%s        %s[%s]%s              %s[%s]%s              %s[%s]%s              %s║%s\n",
           ANSI_CYAN, ANSI_RESET,
           heater_on ? ANSI_RED ANSI_BOLD : ANSI_GRAY, heater_on ? " ON " : "OFF ", ANSI_RESET,
           fan_on ? ANSI_GREEN ANSI_BOLD : ANSI_GRAY, fan_on ? " ON " : "OFF ", ANSI_RESET,
           led_on ? ANSI_YELLOW ANSI_BOLD : ANSI_GRAY, led_on ? " ON " : "OFF ", ANSI_RESET,
           ANSI_CYAN, ANSI_RESET);
    printf("%s║%s                                                                          %s║%s\n", ANSI_CYAN, ANSI_RESET, ANSI_CYAN, ANSI_RESET);
    printf("%s╠══════════════════════════════════════════════════════════════════════════╣%s\n", ANSI_CYAN, ANSI_RESET);
    printf("%s║%s                                                                          %s║%s\n", ANSI_CYAN, ANSI_RESET, ANSI_CYAN, ANSI_RESET);
    
    // ====== ASCII Art 애니메이션 (5줄) ======
    
    // 1번째 줄
    printf("%s║%s", ANSI_CYAN, ANSI_RESET);
    // 히터
    if (heater_on) {
        if (frame % 2 == 0) printf("       %s  (   )  %s", ANSI_ORANGE, ANSI_RESET);
        else printf("       %s (  *  ) %s", ANSI_RED, ANSI_RESET);
    } else {
        printf("       %s         %s", ANSI_GRAY, ANSI_RESET);
    }
    // 팬
    if (fan_on) {
        switch(frame % 4) {
            case 0: printf("          %s   |   %s", ANSI_CYAN, ANSI_RESET); break;
            case 1: printf("          %s \\   / %s", ANSI_CYAN, ANSI_RESET); break;
            case 2: printf("          %s   -   %s", ANSI_CYAN, ANSI_RESET); break;
            case 3: printf("          %s /   \\ %s", ANSI_CYAN, ANSI_RESET); break;
        }
    } else {
        printf("          %s   |   %s", ANSI_GRAY, ANSI_RESET);
    }
    // LED
    if (led_on) {
        if (frame % 2 == 0) printf("           %s .-. %s", ANSI_YELLOW, ANSI_RESET);
        else printf("           %s*.-.*%s", ANSI_BOLD ANSI_YELLOW, ANSI_RESET);
    } else {
        printf("           %s .-. %s", ANSI_GRAY, ANSI_RESET);
    }
    printf("         %s║%s\n", ANSI_CYAN, ANSI_RESET);

    // 2번째 줄
    printf("%s║%s", ANSI_CYAN, ANSI_RESET);
    if (heater_on) {
        if (frame % 2 == 0) printf("       %s ( * * ) %s", ANSI_RED, ANSI_RESET);
        else printf("       %s (  *  ) %s", ANSI_ORANGE, ANSI_RESET);
    } else {
        printf("       %s  ____  %s", ANSI_GRAY, ANSI_RESET);
    }
    if (fan_on) {
        switch(frame % 4) {
            case 0: printf("          %s   |   %s", ANSI_CYAN, ANSI_RESET); break;
            case 1: printf("          %s  \\ /  %s", ANSI_CYAN, ANSI_RESET); break;
            case 2: printf("          %s---*---%s", ANSI_GREEN, ANSI_RESET); break;
            case 3: printf("          %s  / \\  %s", ANSI_CYAN, ANSI_RESET); break;
        }
    } else {
        printf("          %s   |   %s", ANSI_GRAY, ANSI_RESET);
    }
    if (led_on) {
        if (frame % 2 == 0) printf("           %s|@@@|%s", ANSI_BOLD ANSI_YELLOW, ANSI_RESET);
        else printf("           %s|***|%s", ANSI_YELLOW, ANSI_RESET);
    } else {
        printf("           %s|   |%s", ANSI_GRAY, ANSI_RESET);
    }
    printf("         %s║%s\n", ANSI_CYAN, ANSI_RESET);

    // 3번째 줄
    printf("%s║%s", ANSI_CYAN, ANSI_RESET);
    if (heater_on) {
        if (frame % 2 == 0) printf("       %s(* * * *)%s", ANSI_ORANGE, ANSI_RESET);
        else printf("       %s( * * * )%s", ANSI_RED, ANSI_RESET);
    } else {
        printf("       %s /    \\ %s", ANSI_GRAY, ANSI_RESET);
    }
    if (fan_on) {
        switch(frame % 4) {
            case 0: printf("          %s---*---%s", ANSI_GREEN, ANSI_RESET); break;
            case 1: printf("          %s   *   %s", ANSI_GREEN, ANSI_RESET); break;
            case 2: printf("          %s   *   %s", ANSI_GREEN, ANSI_RESET); break;
            case 3: printf("          %s   *   %s", ANSI_GREEN, ANSI_RESET); break;
        }
    } else {
        printf("          %s---o---%s", ANSI_GRAY, ANSI_RESET);
    }
    if (led_on) {
        if (frame % 2 == 0) printf("           %s|@@@|%s", ANSI_BOLD ANSI_YELLOW, ANSI_RESET);
        else printf("           %s*@@@*%s", ANSI_BOLD ANSI_YELLOW, ANSI_RESET);
    } else {
        printf("           %s|   |%s", ANSI_GRAY, ANSI_RESET);
    }
    printf("         %s║%s\n", ANSI_CYAN, ANSI_RESET);

    // 4번째 줄
    printf("%s║%s", ANSI_CYAN, ANSI_RESET);
    if (heater_on) {
        printf("       %s(* * * *)%s", ANSI_RED, ANSI_RESET);
    } else {
        printf("       %s |    | %s", ANSI_GRAY, ANSI_RESET);
    }
    if (fan_on) {
        switch(frame % 4) {
            case 0: printf("          %s   |   %s", ANSI_CYAN, ANSI_RESET); break;
            case 1: printf("          %s  / \\  %s", ANSI_CYAN, ANSI_RESET); break;
            case 2: printf("          %s---*---%s", ANSI_GREEN, ANSI_RESET); break;
            case 3: printf("          %s  \\ /  %s", ANSI_CYAN, ANSI_RESET); break;
        }
    } else {
        printf("          %s   |   %s", ANSI_GRAY, ANSI_RESET);
    }
    if (led_on) {
        if (frame % 2 == 0) printf("           %s '-' %s", ANSI_YELLOW, ANSI_RESET);
        else printf("           %s*'-'*%s", ANSI_YELLOW, ANSI_RESET);
    } else {
        printf("           %s '-' %s", ANSI_GRAY, ANSI_RESET);
    }
    printf("         %s║%s\n", ANSI_CYAN, ANSI_RESET);

    // 5번째 줄 (바닥)
    printf("%s║%s", ANSI_CYAN, ANSI_RESET);
    printf("       %s[======]%s", ANSI_GRAY, ANSI_RESET);
    if (fan_on) {
        switch(frame % 4) {
            case 0: printf("          %s   |   %s", ANSI_CYAN, ANSI_RESET); break;
            case 1: printf("          %s /   \\ %s", ANSI_CYAN, ANSI_RESET); break;
            case 2: printf("          %s   |   %s", ANSI_CYAN, ANSI_RESET); break;
            case 3: printf("          %s \\   / %s", ANSI_CYAN, ANSI_RESET); break;
        }
    } else {
        printf("          %s   |   %s", ANSI_GRAY, ANSI_RESET);
    }
    printf("           %s[===]%s", ANSI_GRAY, ANSI_RESET);
    printf("         %s║%s\n", ANSI_CYAN, ANSI_RESET);

    printf("%s║%s                                                                          %s║%s\n", ANSI_CYAN, ANSI_RESET, ANSI_CYAN, ANSI_RESET);
    printf("%s╚══════════════════════════════════════════════════════════════════════════╝%s\n", ANSI_CYAN, ANSI_RESET);
    printf("\n  %sPID: %d | 0.5초마다 갱신 | Ctrl+C 종료%s\n", ANSI_DIM, getpid(), ANSI_RESET);
    
    // 프레임 증가
    frame++;
}

/* ============================================================================
 * 함수: read_control_state
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

    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);

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

    sleep(1);  // 초기 메시지 보여주기

    // 메인 루프
    while (1) {
        sem_lock(sem_id);
        int running = shared_data->system_running;
        sem_unlock(sem_id);
        if (!running) {
            printf("\033[2J\033[H");
            printf("[ACTUATOR] 서버 종료 신호 수신. 프로세스 종료.\n");
            break;
        }

        read_control_state();
        display_dashboard();

        usleep(500000);  // 0.5초마다 갱신 (애니메이션용)
    }

    return 0;
}
