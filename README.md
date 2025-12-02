# 가상 스마트팜 시스템 (Virtual Smart Farm System)

UNIX 시스템 프로그래밍 기말 프로젝트 - VirtualBox 환경에서 동작하는 완전 가상 스마트팜

## 프로젝트 개요 (Project Overview)

본 프로젝트는 실제 하드웨어 없이 **순수 소프트웨어**로 스마트팜 환경을 시뮬레이션하는 시스템입니다.
물리 엔진을 탑재하여 실제 환경과 유사한 온도/습도 변화를 구현하며, System V IPC를 활용한 프로세스 간 통신을 통해 센서-서버-액추에이터가 유기적으로 동작합니다.

## 팀 구성 (Team Members)

- **팀장 (Team Leader)**: 안민철 (anmincheol-71)
- **팀원 (Team Member)**: 백승찬 (kairos1228)

## 핵심 기술 (Core Technologies)

### 1. 가상 물리 엔진 (Virtual Physics Engine)
- 단순 랜덤 값이 아닌 실제 물리 법칙 기반 시뮬레이션
- 뉴턴 냉각 법칙을 적용한 온도 변화
- 히터/팬 상태에 따른 동적 환경 변화

### 2. System V IPC 완전 활용 (System V IPC Utilization)
- **Message Queue**: 센서 데이터 전송, 제어 명령 전달
- **Shared Memory**: 임계값 설정 공유
- **Semaphore**: Race Condition 방지 동기화

### 3. 모듈화 설계 (Modular Programming)
- 독립적인 4개 프로세스 구조
- 헤더 파일을 통한 공통 인터페이스 관리
- Makefile을 통한 자동화된 빌드 시스템

## 프로젝트 구조 (Project Structure)

```
virtual-smartfarm-unix-system/
├── Makefile              # 빌드 자동화
├── README.md             # 프로젝트 문서
├── include/
│   └── common.h          # 공통 헤더 (IPC 키, 구조체 정의)
├── src/
│   ├── main_sensor.c     # [P1] 가상 센서 프로세스
│   ├── main_actuator.c   # [P2] 액추에이터 제어 프로세스
│   ├── main_server.c     # [P3] 중앙 서버 프로세스
│   └── main_monitor.c    # [P4] 사용자 설정 프로세스
└── bin/                  # 실행 파일 (빌드 후 생성)
    ├── sensor
    ├── actuator
    ├── server
    └── monitor
```

## 빌드 및 실행 (Build & Execution)

### 1. 빌드 (Build)
```bash
make all
```

### 2. 실행 (Execution) - 별도 터미널에서 각각 실행
```bash
# Terminal 1: 중앙 서버 시작 (가장 먼저 실행)
./bin/server

# Terminal 2: 센서 프로세스
./bin/sensor

# Terminal 3: 액추에이터 프로세스
./bin/actuator

# Terminal 4: 설정 모니터
./bin/monitor
```

### 3. 정리 (Clean)
```bash
make clean
```

## 프로세스 상세 설명 (Process Details)

### [P1] Sensor (가상 센서)
- **역할**: 온도/습도 측정 및 물리 시뮬레이션
- **기술**: 가상 물리 엔진, Message Queue 송신
- **특징**:
  - 히터 ON → 온도 0.2°C/초 상승
  - 히터 OFF → 주변 온도(25°C)로 자연 냉각
  - 팬 상태에 따른 습도 변화

### [P2] Actuator (제어 장치)
- **역할**: 히터/팬/LED 제어 및 상태 표시
- **기술**: ANSI Escape Code, Message Queue 수신
- **특징**: 컬러풀한 실시간 대시보드 UI

### [P3] Server (중앙 서버)
- **역할**: 데이터 수집, 로직 처리, 명령 전달
- **기술**: Message Queue, Shared Memory, Semaphore, Signal Handler
- **특징**:
  - 센서 데이터 기반 자동 제어
  - 임계값 초과 시 액추에이터 제어
  - 안전한 자원 정리 (IPC 삭제)

### [P4] Monitor (사용자 설정)
- **역할**: 임계값 설정 및 모니터링
- **기술**: Shared Memory, Semaphore
- **특징**: CLI 메뉴를 통한 대화형 설정

## 채점 기준 대응 (Grading Criteria)

| 항목 | 배점 | 구현 내용 |
|------|------|-----------|
| **난이도 및 유용성** | 3점 | 가상 물리 엔진, ANSI UI, 실시간 시뮬레이션 |
| **소스코드 및 주석** | 3점 | 상세한 한글 주석, 파일별 역할 설명 |
| **컴파일 및 작동** | 3점 | 표준 C 라이브러리만 사용, VirtualBox 완벽 호환 |
| **Make & 모듈화** | 3점 | Makefile, include/src 분리, 독립 모듈 설계 |

## 개발 환경 (Development Environment)

- **OS**: Ubuntu 24.04 (VirtualBox)
- **Compiler**: GCC
- **Language**: C (표준 라이브러리)
- **IPC**: System V (Message Queue, Shared Memory, Semaphore)

## 라이선스 (License)

Educational Project - 2025

---

**작성자 (Authors)**: 안민철, 백승찬
**작성일 (Date)**: 2025-12-02
