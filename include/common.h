/*
 * ==============================================================================
 * 파일명: common.h
 * 역할: 전체 스마트팜 시스템의 공통 헤더 파일
 *
 * 기술 요소:
 *   - System V IPC (Message Queue, Shared Memory, Semaphore) 키 및 구조체 정의
 *   - 프로세스 간 통신을 위한 메시지 구조체 정의
 *   - 공유 메모리 구조체 정의 (임계값, 제어 상태 관리)
 *
 * 작성자: Virtual SmartFarm Team
 * 작성일: 2025-12-02
 * ==============================================================================
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/utsname.h>    // uname() - 시스템 정보
#include <sys/wait.h>       // wait() - 프로세스 대기
#include <pthread.h>        // pthread - 쓰레드
#include <time.h>
#include <errno.h>

/* ============================================================================
 * IPC 키 정의 (System V IPC)
 * ============================================================================ */
#define MSG_KEY_DATA    0x1234      // 센서 데이터 전송용 메시지 큐 키
#define SHM_KEY         0x9ABC      // 공유 메모리 키 (설정값 + 제어 상태 공유)
#define SEM_KEY         0xDEF0      // 세마포어 키 (동기화)

/* ============================================================================
 * 메시지 타입 정의
 * ============================================================================ */
#define MSG_TYPE_SENSOR_DATA    1   // 센서 -> 서버: 센서 데이터

/* ============================================================================
 * 센서 데이터 메시지 구조체
 * - 센서 프로세스(P1)가 서버(P3)로 전송
 * - 온도, 습도, 타임스탬프 포함
 * ============================================================================ */
typedef struct {
    long msg_type;              // 메시지 타입 (MSG_TYPE_SENSOR_DATA)
    float temperature;          // 현재 온도 (섭씨)
    float humidity;             // 현재 습도 (%)
    time_t timestamp;           // 측정 시각
} SensorDataMsg;

/* ============================================================================
 * 공유 메모리 구조체
 * - 서버(P3), 센서(P1), 액추에이터(P2), 모니터(P4)가 공유
 * - 임계값 설정 및 제어 상태 관리
 * - 세마포어로 동기화하여 Race Condition 방지
 *
 * [변경사항] 제어 상태(heater_on, fan_on, led_on)를 공유 메모리로 이동
 *           → 메시지 큐 경쟁 문제 해결
 * ============================================================================ */
typedef struct {
    /* 임계값 설정 (모니터에서 수정, 서버에서 읽기) */
    int temp_threshold;         // 온도 임계값 (기본: 28도)
    int humidity_threshold;     // 습도 임계값 (기본: 70%)

    /* 제어 상태 (서버에서 수정, 센서/액추에이터에서 읽기) */
    int heater_on;              // 히터 상태 (1=ON, 0=OFF)
    int fan_on;                 // 팬 상태 (1=ON, 0=OFF)
    int led_on;                 // LED 상태 (1=ON, 0=OFF)

    /* 현재 센서 데이터 (서버에서 수정, 액추에이터/모니터에서 읽기) */
    float current_temp;         // 현재 온도
    float current_humidity;     // 현재 습도

    /* 시스템 상태 */
    int system_running;         // 시스템 실행 상태 플래그 (0=종료 요청)
} SharedData;

/* ============================================================================
 * 세마포어 연산 구조체 (System V Semaphore)
 * ============================================================================ */
union semun {
    int val;                    // SETVAL용 값
    struct semid_ds *buf;       // IPC_STAT, IPC_SET용 버퍼
    unsigned short *array;      // GETALL, SETALL용 배열
};

/* ============================================================================
 * 함수 프로토타입 - 세마포어 유틸리티
 * ============================================================================ */
// 세마포어 P 연산 (잠금)
static inline void sem_lock(int sem_id) {
    struct sembuf sb = {0, -1, 0};  // 세마포어 0번을 1 감소 (잠금)
    if (semop(sem_id, &sb, 1) == -1) {
        perror("sem_lock failed");
    }
}

// 세마포어 V 연산 (해제)
static inline void sem_unlock(int sem_id) {
    struct sembuf sb = {0, 1, 0};   // 세마포어 0번을 1 증가 (해제)
    if (semop(sem_id, &sb, 1) == -1) {
        perror("sem_unlock failed");
    }
}

/* ============================================================================
 * 디버그 매크로
 * ============================================================================ */
#define DEBUG_PRINT(fmt, ...) \
    do { \
        fprintf(stderr, "[DEBUG][%s:%d] " fmt "\n", \
                __FILE__, __LINE__, ##__VA_ARGS__); \
    } while(0)

#endif /* COMMON_H */
