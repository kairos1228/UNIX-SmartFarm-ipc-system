# 가상 스마트팜 시스템 (Virtual Smart Farm System)

UNIX 시스템 프로그래밍 기말 프로젝트 - VirtualBox 환경에서 동작하는 완전 가상 스마트팜

## 프로젝트 개요 (Project Overview)

본 프로젝트는 실제 하드웨어 없이 **순수 소프트웨어**로 스마트팜 환경을 시뮬레이션하는 시스템입니다.
UNIX 시스템 프로그래밍의 핵심 개념들을 종합적으로 활용하여 구현하였습니다.

## 팀 구성 (Team Members)

- **팀장 (Team Leader)**: 안민철 (anmincheol-71)
- **팀원 (Team Member)**: 백승찬 (kairos1228)

---

## 시스템 아키텍처 (System Architecture)

![시스템 아키텍처](docs/UNIX_smartfarm_architecture.png)

---

## 사용된 UNIX 기술 (UNIX Technologies)

| 주차 | 기술 | 사용 위치 | 설명 |
|------|------|----------|------|
| 5-6주 | **파일 I/O** | 서버 | 로그 파일 저장 (fopen, fprintf) |
| 7주 | **시스템/프로세스 정보** | 전체 | uname(), getpid(), getppid(), getuid() |
| 9주 | **프로세스 생성 (fork)** | 서버 | 로그 기록 자식 프로세스 |
| 10주 | **쓰레드 (pthread)** | 서버 | 경고 모니터링 스레드 |
| 11주 | **시그널 (signal)** | 전체 | SIGINT/SIGTERM 처리 |
| 12주 | **파이프 (pipe)** | 서버 | 부모-자식 간 로그 데이터 전송 |
| 13주 | **System V IPC** | 전체 | Message Queue, Shared Memory, Semaphore |

---

## 프로젝트 구조 (Project Structure)

```
virtual-smartfarm/
├── Makefile              # 빌드 자동화 (pthread 링크)
├── README.md             # 프로젝트 문서
├── include/
│   └── common.h          # 공통 헤더 (IPC 키, 구조체)
├── src/
│   ├── main_sensor.c     # [P1] 가상 센서 프로세스
│   ├── main_actuator.c   # [P2] 액추에이터 프로세스
│   ├── main_server.c     # [P3] 중앙 서버 (fork, pipe, pthread)
│   └── main_monitor.c    # [P4] 설정/모니터링 (select)
├── bin/                  # 실행 파일 (빌드 후 생성)
└── smartfarm.log         # 로그 파일 (실행 후 생성)
```

---

## 빌드 및 실행 (Build & Execution)

### 빌드
```bash
make all
```

### 실행 (4개 터미널에서 순서대로)
```bash
# Terminal 1: 서버 (먼저 실행 - IPC 자원 생성)
./bin/server

# Terminal 2: 센서
./bin/sensor

# Terminal 3: 액추에이터
./bin/actuator

# Terminal 4: 모니터
./bin/monitor
```

### 종료
```bash
# 서버 터미널에서 Ctrl+C → 모든 프로세스 자동 종료
```

---

## 프로세스 상세 (Process Details)

### [P1] Sensor - 가상 센서
- **물리 엔진**: 뉴턴 냉각 법칙 기반 온도/습도 시뮬레이션
- **IPC**: Message Queue 송신, Shared Memory 읽기

### [P2] Actuator - 제어 장치
- **ANSI UI**: 컬러 대시보드 (온도/습도/장치 상태)
- **IPC**: Shared Memory 읽기

### [P3] Server - 중앙 서버
- **fork()**: 로그 기록 자식 프로세스
- **pipe()**: 부모→자식 로그 데이터 전송
- **pthread**: 경고 모니터링 스레드
- **IPC**: Message Queue, Shared Memory, Semaphore

### [P4] Monitor - 설정/모니터링
- **select()**: 논블로킹 입력 (종료 신호 감지)
- **uname()**: 시스템 정보 조회
- **IPC**: Shared Memory, Semaphore

---

## 개발 환경 (Development Environment)

- **OS**: Ubuntu 24.04 (VirtualBox)
- **Compiler**: GCC
- **Language**: C (표준 라이브러리 + pthread)

---

**작성자**: 안민철, 백승찬  
**작성일**: 2025-12-04
