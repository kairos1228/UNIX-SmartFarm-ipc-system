/*
 * ==============================================================================
 * 파일명: main_actuator.c
 * 역할: [P2] 액추에이터 제어 프로세스 - ANSI UI Dashboard
 *
 * 기술 요소:
 *   - ANSI Escape Code를 사용한 컬러 터미널 UI
 *   - Message Queue를 통한 제어 명령 수신
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
#define ANSI_BOLD    "\x1b[1m"

static int control_queue_id = -1;
static int heater_on = 0;
static int fan_on = 0;
static int led_on = 0;

void cleanup_and_exit(int signo) {
    printf("\n[ACTUATOR] 종료 중...\n");
    exit(0);
}

void display_dashboard() {
    system("clear");
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║     " ANSI_BOLD ANSI_CYAN "🌱 SMART FARM ACTUATOR DASHBOARD 🌱" ANSI_RESET "        ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║                                                            ║\n");
    printf("║  🔥 히터(Heater):   %s%-6s%s                            ║\n",
           heater_on ? ANSI_RED : ANSI_BLUE,
           heater_on ? "[ON]" : "[OFF]",
           ANSI_RESET);
    printf("║  💨 팬(Fan):        %s%-6s%s                            ║\n",
           fan_on ? ANSI_GREEN : ANSI_BLUE,
           fan_on ? "[ON]" : "[OFF]",
           ANSI_RESET);
    printf("║  💡 LED:           %s%-6s%s                            ║\n",
           led_on ? ANSI_YELLOW : ANSI_BLUE,
           led_on ? "[ON]" : "[OFF]",
           ANSI_RESET);
    printf("║                                                            ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

int main() {
    printf("[ACTUATOR] 프로세스 시작\n");

    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);

    control_queue_id = msgget(MSG_KEY_CONTROL, 0666 | IPC_CREAT);
    if (control_queue_id == -1) {
        perror("[ACTUATOR] 메시지 큐 생성 실패");
        exit(1);
    }

    while (1) {
        ControlCmdMsg cmd_msg;
        ssize_t result = msgrcv(control_queue_id, &cmd_msg,
                               sizeof(ControlCmdMsg) - sizeof(long),
                               MSG_TYPE_CONTROL_CMD, IPC_NOWAIT);

        if (result != -1) {
            heater_on = cmd_msg.heater_on;
            fan_on = cmd_msg.fan_on;
            led_on = cmd_msg.led_on;
        }

        display_dashboard();
        sleep(1);
    }

    return 0;
}
