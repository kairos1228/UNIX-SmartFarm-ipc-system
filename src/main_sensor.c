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
 *
 * 물리 모델:
 *   - 히터 ON: 온도가 0.2도/초 상승
 *   - 히터 OFF: 주변 온도(25도)로 0.1도/초 하강
 *   - 팬 ON: 습도가 0.5%/초 감소
 *   - 팬 OFF: 습도가 0.3%/초 증가
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
static int msg_queue_id = -1;          // 메시지 큐 ID
static int control_queue_id = -1;      // 제어 메시지 큐 ID

/* ============================================================================
 * 함수: cleanup_and_exit
 * 설명: 시그널 핸들러 - 프로세스 종료 시 자원 정리
 * ============================================================================ */
void cleanup_and_exit(int signo) {
    printf("\n[SENSOR] 종료 신호 수신. 프로세스 종료 중...\n");
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
 * 함수: check_control_commands
 * 설명: 서버로부터 제어 명령을 수신하여 히터/팬 상태 업데이트
 *       - 논블로킹 방식(IPC_NOWAIT)으로 메시지 확인
 * ============================================================================ */
void check_control_commands() {
    ControlCmdMsg cmd_msg;

    // msgrcv: 메시지 큐에서 제어 명령 수신 (논블로킹)
    // - control_queue_id: 제어 메시지 큐 ID
    // - &cmd_msg: 수신 버퍼
    // - sizeof(ControlCmdMsg) - sizeof(long): 메시지 데이터 크기
    // - MSG_TYPE_CONTROL_CMD: 제어 명령 타입만 수신
    // - IPC_NOWAIT: 메시지가 없어도 블로킹하지 않음
    ssize_t result = msgrcv(control_queue_id, &cmd_msg,
                           sizeof(ControlCmdMsg) - sizeof(long),
                           MSG_TYPE_CONTROL_CMD, IPC_NOWAIT);

    if (result != -1) {
        // 제어 명령 수신 성공
        heater_state = cmd_msg.heater_on;
        fan_state = cmd_msg.fan_on;
        printf("[SENSOR] 제어 명령 수신 - 히터:%s, 팬:%s\n",
               heater_state ? "ON" : "OFF",
               fan_state ? "ON" : "OFF");
    }
    // 메시지가 없으면 현재 상태 유지
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
    // - msg_queue_id: 데이터 메시지 큐 ID
    // - &sensor_msg: 전송할 메시지
    // - sizeof(SensorDataMsg) - sizeof(long): 메시지 데이터 크기
    // - 0: 블로킹 모드 (큐가 가득 차면 대기)
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
    printf("  - Message Queue를 통한 데이터 전송\n");
    printf("==================================================\n\n");

    // 시그널 핸들러 등록 (Ctrl+C 처리)
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);

    // 난수 생성기 초기화 (미세 노이즈용)
    srand(time(NULL));

    // ========================================================================
    // Message Queue 연결
    // ========================================================================

    // msgget: 센서 데이터 전송용 메시지 큐 생성/연결
    // - MSG_KEY_DATA: common.h에 정의된 키
    // - 0666 | IPC_CREAT: 읽기/쓰기 권한, 없으면 생성
    msg_queue_id = msgget(MSG_KEY_DATA, 0666 | IPC_CREAT);
    if (msg_queue_id == -1) {
        perror("[SENSOR] 데이터 메시지 큐 생성 실패");
        exit(1);
    }
    printf("[SENSOR] 데이터 메시지 큐 연결 성공 (ID: %d)\n", msg_queue_id);

    // msgget: 제어 명령 수신용 메시지 큐 생성/연결
    control_queue_id = msgget(MSG_KEY_CONTROL, 0666 | IPC_CREAT);
    if (control_queue_id == -1) {
        perror("[SENSOR] 제어 메시지 큐 생성 실패");
        exit(1);
    }
    printf("[SENSOR] 제어 메시지 큐 연결 성공 (ID: %d)\n\n", control_queue_id);

    // ========================================================================
    // 메인 루프: 0.5초마다 물리 시뮬레이션 및 1초마다 데이터 전송
    // ========================================================================
    int loop_count = 0;
    while (1) {
        // 제어 명령 확인 (히터/팬 상태 업데이트)
        check_control_commands();

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
