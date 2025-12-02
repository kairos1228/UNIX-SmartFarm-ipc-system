/*
 * ==============================================================================
 * 파일명: main_monitor.c
 * 역할: [P4] 사용자 모니터링 및 설정 프로세스
 *
 * 기술 요소:
 *   - Shared Memory: 임계값 설정 읽기/쓰기
 *   - Semaphore: Race Condition 방지를 위한 동기화
 *   - CLI 메뉴 인터페이스
 *
 * 작성자: Virtual SmartFarm Team
 * 작성일: 2025-12-02
 * ==============================================================================
 */

#include "../include/common.h"

static int shm_id = -1;
static int sem_id = -1;
static SharedData *shared_data = NULL;

void cleanup_and_exit(int signo) {
    printf("\n[MONITOR] 종료 중...\n");
    if (shared_data != NULL) {
        shmdt(shared_data);
    }
    exit(0);
}

void display_menu() {
    printf("\n");
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║   🎛️  SMART FARM CONFIGURATION MENU 🎛️       ║\n");
    printf("╠════════════════════════════════════════════════╣\n");
    printf("║  1. 온도 임계값 설정                          ║\n");
    printf("║  2. 습도 임계값 설정                          ║\n");
    printf("║  3. 현재 설정 확인                            ║\n");
    printf("║  4. 종료                                      ║\n");
    printf("╚════════════════════════════════════════════════╝\n");
    printf("선택: ");
}

int main() {
    printf("[MONITOR] 프로세스 시작\n");

    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);

    shm_id = shmget(SHM_KEY, sizeof(SharedData), 0666);
    sem_id = semget(SEM_KEY, 1, 0666);

    if (shm_id == -1 || sem_id == -1) {
        perror("[MONITOR] IPC 자원 접근 실패");
        exit(1);
    }

    shared_data = (SharedData *)shmat(shm_id, NULL, 0);
    if (shared_data == (void *)-1) {
        perror("[MONITOR] 공유 메모리 연결 실패");
        exit(1);
    }

    int choice;
    while (1) {
        display_menu();
        scanf("%d", &choice);

        switch (choice) {
            case 1: {
                int new_temp;
                printf("새로운 온도 임계값 (°C): ");
                scanf("%d", &new_temp);
                sem_lock(sem_id);
                shared_data->temp_threshold = new_temp;
                sem_unlock(sem_id);
                printf("✓ 온도 임계값이 %d°C로 설정되었습니다.\n", new_temp);
                break;
            }
            case 2: {
                int new_hum;
                printf("새로운 습도 임계값 (%%): ");
                scanf("%d", &new_hum);
                sem_lock(sem_id);
                shared_data->humidity_threshold = new_hum;
                sem_unlock(sem_id);
                printf("✓ 습도 임계값이 %d%%로 설정되었습니다.\n", new_hum);
                break;
            }
            case 3:
                sem_lock(sem_id);
                printf("\n[현재 설정]\n");
                printf("  온도 임계값: %d°C\n", shared_data->temp_threshold);
                printf("  습도 임계값: %d%%\n", shared_data->humidity_threshold);
                sem_unlock(sem_id);
                break;
            case 4:
                cleanup_and_exit(0);
                break;
            default:
                printf("잘못된 선택입니다.\n");
        }
    }

    return 0;
}
