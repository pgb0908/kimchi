# TCP 처리 구조

HTTP 외에 raw TCP 트래픽을 처리하기 위한 구조를 설명합니다.
wangle의 파이프라인 기반 아키텍처를 사용합니다.

---

## HTTP vs TCP 처리 비교

```
         HTTP (현재)                        TCP (추가)
──────────────────────────────    ────────────────────────────────
         클라이언트                          클라이언트
             │                                   │
        TCP 연결                            TCP 연결
             │                                   │
      proxygen HTTPServer              wangle ServerBootstrap
             │                                   │
    HTTPSession / Transaction            Pipeline 생성
             │                                   │
   RequestHandlerFactory                 PipelineFactory
             │                                   │
  Filter → Filter → Handler      Decoder → Encoder → Handler
             │                                   │
         응답 전송                           응답 전송
```

proxygen은 HTTP 프로토콜을 전제로 설계되어 있어 raw TCP에는 적합하지 않습니다.
TCP는 wangle의 파이프라인을 직접 사용합니다.

---

## wangle 파이프라인 구조

wangle의 파이프라인은 Netty(Java)와 동일한 개념입니다.
소켓에서 읽은 바이트가 핸들러 체인을 순서대로 통과합니다.

```
소켓 (raw bytes)
      │
      ▼
┌─────────────────────────────────────────────────────┐
│                      Pipeline                       │
│                                                     │
│  ┌──────────────────┐                               │
│  │ AsyncSocketHandler│  소켓 읽기/쓰기 (wangle 내장) │
│  └────────┬─────────┘                               │
│           │ IOBuf (raw bytes)                       │
│           ▼                                         │
│  ┌──────────────────┐                               │
│  │   FrameDecoder   │  바이트 → 메시지 객체          │
│  └────────┬─────────┘                               │
│           │ MyMessage (구조체)                       │
│           ▼                                         │
│  ┌──────────────────┐                               │
│  │    MyHandler     │  비즈니스 로직                 │
│  └────────┬─────────┘                               │
│           │                                         │
└───────────┼─────────────────────────────────────────┘
            │
            ▼ (응답)
         클라이언트
```

---

## 데이터 흐름

### 수신 (클라이언트 → 앱)

```
클라이언트
    │  raw bytes
    ▼
AsyncSocketHandler.read()
    │  folly::IOBuf
    ▼
FrameDecoder.decode()          ← 불완전한 패킷이면 다음 수신까지 대기
    │  MyMessage (완성된 메시지)
    ▼
MyHandler.read()               ← 비즈니스 로직 실행
```

### 송신 (앱 → 클라이언트)

```
MyHandler.write(response)
    │  MyResponse
    ▼
FrameEncoder.encode()          ← 구조체 → 바이트 직렬화
    │  folly::IOBuf
    ▼
AsyncSocketHandler.write()
    │  raw bytes
    ▼
클라이언트
```

---

## 프레이밍 방식

TCP는 스트림 프로토콜이라 메시지 경계가 없습니다.
수신된 바이트를 의미 있는 메시지 단위로 자르는 것이 **프레이밍**입니다.

### 방식 1: 길이 접두사 (Length-Prefixed)

```
┌──────────┬────────────────────────────────┐
│  4 bytes │          payload               │
│  (길이)  │          (가변 길이)            │
└──────────┴────────────────────────────────┘

  1. 먼저 4바이트를 읽어 payload 길이 파악
  2. 해당 길이만큼 추가로 읽어 메시지 완성
  3. 길이가 부족하면 버퍼에 보관 후 다음 수신 대기
```

→ wangle 내장 `LengthFieldBasedFrameDecoder` 사용 가능

### 방식 2: 구분자 (Delimiter)

```
┌──────────────────────────────────┬────┐
│             payload              │ \n │
└──────────────────────────────────┴────┘

  구분자(\n 또는 \r\n)를 만날 때까지 버퍼에 누적
```

→ wangle 내장 `LineBasedFrameDecoder` 사용 가능

### 방식 3: 고정 길이 (Fixed-Length)

```
┌──────────────────────────────────┐
│          항상 N bytes            │
└──────────────────────────────────┘

  N바이트가 모일 때까지 대기 후 메시지 완성
```

→ wangle 내장 `FixedLengthFrameDecoder` 사용 가능

### 방식 4: 커스텀 헤더

```
┌───────┬───────┬──────┬────────────────────┐
│ magic │  ver  │ len  │      payload       │
│ 2byte │ 1byte │ 4byte│    (len bytes)     │
└───────┴───────┴──────┴────────────────────┘

  ByteToMessageDecoder<T>를 상속해 decode() 직접 구현
```

---

## 세션 컨트롤

wangle은 연결(세션) 단위 제어를 완전히 지원합니다.

### 연결 생명주기

```
클라이언트 접속
      │
      ▼
PipelineFactory::newPipeline()      ← 파이프라인 생성, 핸들러 등록
      │
      ▼
MyHandler::transportActive()        ← 연결 수립 콜백
      │
      │  ←── read() / write() 반복 ──→
      │
      ▼
MyHandler::transportInactive()      ← 연결 종료 콜백
      │
      ▼
Pipeline 소멸
```

### 세션 제어 기능

```
연결 종료
    ctx->getTransport()->closeNow()          즉시 종료
    ctx->getTransport()->close()             Graceful 종료

서버 → 클라이언트 푸시 (요청 없이 서버가 먼저 전송)
    ctx->fireWrite(response)

읽기 흐름 제어
    ctx->getTransport()->pauseReads()        읽기 일시 중지
    ctx->getTransport()->resumeReads()       읽기 재개

연결 목록 관리
    PipelineFactory 안에 map<id, Pipeline*> 유지
    transportActive()   → map 등록
    transportInactive() → map 제거
```

### Idle Timeout 설정

```
Pipeline 핸들러 순서:

  AsyncSocketHandler
       │
  IdleConnTimeoutHandler    ← N초 동안 데이터 없으면 연결 자동 종료 (wangle 내장)
       │
  FrameDecoder
       │
  MyHandler
```

---

## 프로젝트 적용 구조

### 서버 구성

```
main.cpp
  │
  ├── GatewayServer::start()      HTTP (proxygen)   현재
  ├── AdminServer::start()        HTTP (proxygen)   현재
  └── TcpServer::start()          TCP  (wangle)     추가
```

### ListenerConfig 연동

기존 설정 모델의 `protocol` 필드로 분기합니다.

```
config/listener-tcp.json
  {
    "protocol": "TCP",
    "port": 9000,
    "host": "0.0.0.0"
  }

main.cpp 분기:
  protocol == "HTTP"  →  GatewayServer (proxygen)
  protocol == "TCP"   →  TcpServer     (wangle)    ← 추가
```

### 파일 구조

```
src/
  ├── server/
  │     ├── gateway_server.{h,cpp}          현재
  │     ├── admin_server.{h,cpp}            현재
  │     └── tcp_server.{h,cpp}              신규: ServerBootstrap 래퍼
  │
  └── tcp/
        ├── tcp_pipeline_factory.{h,cpp}    신규: Pipeline 조립
        ├── frame_decoder.{h,cpp}           신규: 프레이밍 규칙
        └── tcp_handler.{h,cpp}             신규: 비즈니스 로직
```

### CMake 변경

wangle이 이미 conan으로 설치되어 있어 링크만 추가합니다.

```cmake
find_package(wangle REQUIRED)

target_link_libraries(kimchi
    PRIVATE
        proxygen::proxygenhttpserver
        wangle::wangle                 # 추가
)
```
