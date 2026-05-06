# kimchi gateway

HTTP 게이트웨이 데이터 플레인. Proxygen 기반으로 Router 매칭, Upstream 프록시, 필터 체인을 제공한다.

---

## 빌드

### 사전 요구사항

- CMake 3.24+
- GCC 11+ (C++20)
- Conan 2.9+ (`conan/bin/conan`)

### 빌드 절차

```bash
# 1. CMake 구성 (Conan 의존성 자동 설치)
cmake -B cmake-build-debug -S .

# 2. 컴파일
cmake --build cmake-build-debug --target kimchi
```

빌드 결과물: `cmake-build-debug/src/kimchi`

---

## 실행

### 기본 실행

```bash
./cmake-build-debug/src/kimchi
```

기본값:
- 데이터 플레인: `0.0.0.0:18000`
- Admin API: `0.0.0.0:18001`
- 설정 디렉토리: `/etc/kimchi/config`

### 옵션

| 플래그 | 기본값 | 설명 |
|--------|--------|------|
| `--config_dir` | `/etc/kimchi/config` | JSON 설정 파일 디렉토리 |
| `--data_port_http` | `18000` | 데이터 플레인 HTTP 포트 |
| `--admin_port` | `18001` | Admin API 포트 |

```bash
./cmake-build-debug/src/kimchi \
  --config_dir=./conf \
  --data_port_http=18000 \
  --admin_port=18001
```

### 로그 제어 (glog 플래그)

```bash
# 콘솔 출력 끄기
./cmake-build-debug/src/kimchi --config_dir=./conf --logtostderr=0

# 로그 레벨 조정 (0=INFO, 1=WARNING, 2=ERROR)
./cmake-build-debug/src/kimchi --config_dir=./conf --minloglevel=1
```

종료: `Ctrl+C` 또는 `SIGTERM`

---

## 설정 파일

`--config_dir`로 지정한 디렉토리 안의 모든 `.json` 파일을 읽는다. 파일별로 `kind` 필드로 리소스 종류를 구분한다.

### Listener

포트 바인딩 정의. 없으면 `--data_port_http` 기본값 사용.

```json
{
  "apiVersion": "v1",
  "kind": "Listener",
  "metadata": { "name": "http-listener" },
  "spec": {
    "protocol": "HTTP",
    "port": 18000,
    "host": "0.0.0.0"
  }
}
```

### Router

요청 매칭 규칙과 upstream 대상 정의. `rules[].path`는 정규식을 사용한다.

```json
{
  "apiVersion": "v1",
  "kind": "Router",
  "metadata": { "name": "api-router" },
  "spec": {
    "targetRef": { "kind": "Listener", "name": "http-listener" },
    "rules": [
      {
        "path": "/api(/.*)?",
        "methods": []
      }
    ],
    "config": {
      "destinations": [
        {
          "destinationRef": { "kind": "Service", "name": "my-svc" },
          "weight": 100,
          "rewrite": { "path": "/v1/api" }
        }
      ]
    }
  }
}
```

- `rules[].methods`: 빈 배열이면 전체 메서드 허용
- `rewrite.path`: upstream으로 전달할 경로 (쿼리스트링은 원본 유지)
- `rewrite.host`: upstream에 보낼 Host 헤더 (생략 시 Service 타겟의 host:port 사용)

### Service

Upstream 백엔드 클러스터 정의.

```json
{
  "apiVersion": "v1",
  "kind": "Service",
  "metadata": { "name": "my-svc" },
  "spec": {
    "protocol": "HTTP",
    "loadBalancing": {
      "algorithm": "ROUND_ROBIN",
      "targets": [
        { "host": "10.0.1.10", "port": 8080, "weight": 1 }
      ]
    }
  }
}
```

---

## Admin API

| 엔드포인트 | 메서드 | 설명 |
|------------|--------|------|
| `/healthz` | GET | 헬스 체크 |
| `/config/reload` | POST | 설정 파일 재로드 |

```bash
# 헬스 체크
curl http://localhost:18001/healthz

# 설정 재로드 (서버 재시작 없이 반영)
curl -X POST http://localhost:18001/config/reload
```

---

## 요청 처리 파이프라인

```
Client
  → HeaderFilter     (x-request-id / x-trace-id 생성)
  → AuthFilter       (x-api-key 또는 Authorization 헤더 필수)
  → RateLimitFilter  (quota 체크, 현재 NullStore — 항상 허용)
  → GatewayHandler   (Router 매칭 → Upstream 프록시)
```

매칭 실패: `404 Not Found`  
Service 없음: `502 Bad Gateway`  
인증 없음: `401 Unauthorized`

---

## Proxygen 요청 처리 흐름

### 전체 흐름 도식

```
                        ┌─────────────────────────────────────────────────────────────┐
                        │  kimchi process                                             │
                        │                                                             │
                        │   main()                                                    │
                        │    ├─ ConfigLoader::loadFromDirectory()                     │
                        │    ├─ AdminServer  :18001                                   │
                        │    └─ GatewayServer:18000                                   │
                        │         └─ HTTPServer (N I/O threads)                       │
                        │              ├─ Thread-0  EventBase (epoll)                 │
                        │              ├─ Thread-1  EventBase (epoll)                 │
                        │              └─ Thread-N  EventBase (epoll)                 │
                        └──────────────────────┬──────────────────────────────────────┘
                                               │
          ① TCP SYN                            │ accept()
  ┌────────┐                        ┌──────────▼──────────────────────────────────────┐
  │ Client │ ──────────────────────►│ HTTPDownstreamSession  (I/O Thread에 고정)       │
  │        │                        │  └─ HTTP/1.1 코덱 (바이트 → HTTPMessage 파싱)    │
  └────────┘                        └──────────┬──────────────────────────────────────┘
      ▲                                         │ ② 헤더 파싱 완료
      │                                         ▼
      │                             ┌───────────────────────┐
      │                             │ GatewayHandlerFactory │
      │                             │   ::onRequest()       │  ③ 필터 체인 조립
      │                             └───────────┬───────────┘
      │                                         │
      │                             ┌───────────▼───────────────────────────────────┐
      │                             │                필터 체인                       │
      │                             │                                               │
      │      onRequest(msg) ──────► │  HeaderFilter   x-request-id/trace-id 주입   │
      │      onBody(buf)    ──────► │      │                                        │
      │      onEOM()        ──────► │      ▼                                        │
      │                             │  AuthFilter     x-api-key / Authorization     │
      │                             │      │ 인증 실패 ──────────────────────────────┼──► 401
      │                             │      ▼                                        │
      │                             │  RateLimitFilter  quota 확인                  │
      │                             │      │ 한도 초과 ──────────────────────────────┼──► 429
      │                             │      ▼                                        │
      │                             │  GatewayHandler                               │
      │                             │      │ 라우터 미매칭 ──────────────────────────┼──► 404
      │                             │      │ 서비스 없음  ──────────────────────────┼──► 502
      │                             └──────┼────────────────────────────────────────┘
      │                                    │ ④ UpstreamClient 생성 및 connect()
      │                                    ▼
      │                             ┌─────────────────────────────────────────────┐
      │                             │ UpstreamClient                              │
      │                             │                                             │
      │                             │  HTTPConnector::connect(host, port, 5s)     │
      │                             │      │                                      │
      │                             │      ▼ connectSuccess(session)              │
      │                             │  HTTPTransaction                            │
      │                             │      ├─ sendHeaders(upstreamReq)            │
      │                             │      ├─ sendBody(requestBody_)              │
      │                             │      └─ sendEOM()                           │
      │                             └──────────────────┬──────────────────────────┘
      │                                                │
      │                                    ┌───────────▼──────────┐
      │                                    │   Upstream 서버       │
      │                                    │  (Service targets[0]) │
      │                                    └───────────┬──────────┘
      │                                                │ ⑤ HTTP 응답 스트리밍
      │                             ┌──────────────────▼──────────────────────────┐
      │                             │ UpstreamClient 콜백                          │
      │                             │  onHeadersComplete → downstream_->sendHeaders│
      │                             │  onBody           → downstream_->sendBody    │
      │                             │  onEOM            → downstream_->sendEOM     │
      │                             │  detachTransaction → delete this             │
      │                             └──────────────────┬──────────────────────────┘
      │                                                │
      └────────────────────────────────────────────────┘
                          ⑥ 응답을 클라이언트로 스트리밍
```

**객체 수명 타임라인 (단일 요청)**

```
시간 ──────────────────────────────────────────────────────────────────►

  HandlerFactory::onRequest()
    │
    ├─[생성] HeaderFilter, AuthFilter, RateLimitFilter, GatewayHandler
    │
    ▼
  onRequest(msg) → onBody(buf)... → onEOM()
                                       │
                                       ├─[생성] UpstreamClient
                                       │          │
                                       │     connectSuccess()
                                       │          │ sendHeaders / sendBody / sendEOM
                                       │          │
                                       │     onHeadersComplete / onBody / onEOM
                                       │          │  downstream_->send*  (응답 스트리밍)
                                       │          │
                                       │     detachTransaction()
                                       │         [소멸] UpstreamClient
                                       │
                                  requestComplete()
                                      [소멸] GatewayHandler
                                      [소멸] RateLimitFilter
                                      [소멸] AuthFilter
                                      [소멸] HeaderFilter
```

---

### 1. 서버 초기화 — 스레드와 EventBase

`GatewayServer` 생성자에서 `proxygen::HTTPServer`에 `HTTPServerOptions`를 넘긴다.

```
main()
  └─ GatewayServer(listeners, store)
       └─ HTTPServer::bind(ipConfigs)   ← 소켓 바인딩만, 아직 listen 안 함
  └─ GatewayServer::start()
       └─ (별도 스레드) HTTPServer::start()
            ├─ CPU 코어 수만큼 I/O 스레드 생성
            ├─ 각 스레드마다 folly::EventBase 하나씩 할당
            └─ 각 EventBase에서 accept loop 시작
```

`opts.threads = hardware_concurrency()`로 설정했으므로 코어 수만큼 I/O 스레드가 뜬다.
각 스레드는 자신의 `EventBase`(epoll 루프)를 독립적으로 돌린다.
연결이 accept되면 그 스레드의 EventBase에 고정되어 요청 처리가 끝날 때까지 같은 스레드에서 실행된다.

---

### 2. 연결 수립 — Accept → HTTPSession

```
클라이언트 TCP SYN
  └─ EventBase::accept callback
       └─ HTTPServer가 AsyncSocket 생성
            └─ HTTPDownstreamSession 생성
                 ├─ HTTP/1.1 코덱 연결
                 └─ 해당 EventBase에서 read 이벤트 대기
```

`HTTPDownstreamSession`이 소켓 수준의 read/write를 담당한다.
여러 요청이 keep-alive로 들어오면 같은 세션 위에서 순서대로 처리된다.

---

### 3. 요청 수신 — HTTPMessage 조립

```
소켓 read 이벤트 발생
  └─ HTTPDownstreamSession::readDataAvailable()
       └─ HTTP 코덱이 바이트 스트림을 파싱
            ├─ 헤더 완성 → HTTPTransaction 생성
            │    └─ RequestHandlerAdaptor 생성
            │         └─ HandlerFactory::onRequest() 호출  ← 필터 체인 조립 시점
            ├─ onRequest(HTTPMessage)  ← 헤더 전달
            ├─ onBody(IOBuf)           ← 바디 청크마다 (없으면 생략)
            └─ onEOM()                 ← 요청 끝 신호
```

`HTTPTransaction`이 한 요청/응답 쌍의 수명을 관리한다.
`RequestHandlerAdaptor`가 `HTTPTransaction`의 콜백을 `RequestHandler` 인터페이스로 번역해준다.

---

### 4. 필터 체인 조립 — GatewayHandlerFactory::onRequest

헤더가 들어오는 순간(파싱 완료 직후) `GatewayHandlerFactory::onRequest`가 호출되어 체인을 조립한다.

```cpp
// gateway_handler_factory.cpp
auto* handler = new GatewayHandler(store_);   // 최종 핸들러
auto* rate    = new RateLimitFilter(handler); // handler를 next로 감쌈
auto* auth    = new AuthFilter(rate);         // rate를 next로 감쌈
auto* hdr     = new HeaderFilter(auth);       // auth를 next로 감쌈
return hdr;                                   // 입구 반환
```

Proxygen은 반환된 `hdr`(입구 필터)을 요청의 첫 번째 수신자로 등록한다.
각 `Filter`는 `proxygen::Filter`를 상속하며 `next_` 포인터로 다음 단계를 가리킨다.

```
onRequest(msg)  흐름 방향 →
  HeaderFilter → AuthFilter → RateLimitFilter → GatewayHandler

blockRequest() 호출 시 흐름 차단:
  FilterBase::blocked_ = true
  → onBody / onEOM이 next_로 전달되지 않음
  → downstream_->sendWithEOM()으로 즉시 에러 응답
```

`downstream_`은 `ResponseHandler*`로, 필터 입장에서 "클라이언트 방향" 포인터다.
`Filter::onRequest`를 호출하면 `next_->onRequest`로 위임된다.

---

### 5. 각 필터의 처리

**HeaderFilter** (`filter/header_filter.cpp`)

```
onRequest(msg)
  ├─ x-request-id 없으면 128-bit hex 랜덤 생성
  ├─ x-trace-id = x-request-id (없으면)
  └─ Filter::onRequest(msg)  → AuthFilter로 전달
```

**AuthFilter** (`filter/auth_filter.cpp`)

```
onRequest(msg)
  ├─ x-api-key 또는 Authorization 헤더 존재 확인
  │    없으면: blockRequest(401)  ← 체인 종료
  ├─ 인증 방식 기록: x-auth-method, x-auth-subject
  ├─ x-gateway-decision: "pass"
  └─ Filter::onRequest(msg)  → RateLimitFilter로 전달
```

**RateLimitFilter** (`filter/rate_limit_filter.cpp`)

```
onRequest(msg)
  ├─ RateLimitStore::checkAndDecrement({tenantId, routeId}) 호출
  │    허용 안 됨: blockRequest(429)  ← 체인 종료
  └─ Filter::onRequest(msg)  → GatewayHandler로 전달
```

현재 `NullRateLimitStore`는 항상 `true`를 반환한다.

---

### 6. GatewayHandler — Router 매칭과 Upstream 디스패치

```
onRequest(msg)
  └─ requestHeaders_ 저장, 로깅

onBody(buf)
  └─ requestBody_에 IOBuf 체인으로 append (POST/PUT 바디 버퍼링)

onEOM()
  ├─ store_->routers 순회
  │    각 rule.path를 std::regex_match(path, regex(rule.path))로 검사
  │    매칭 실패 → 404 sendWithEOM(), return
  ├─ destinations[0].destinationName으로 ServiceConfig 조회
  │    서비스 없음 → 502 sendWithEOM(), return
  ├─ upstream HTTPMessage 조립
  │    ├─ 메서드, URL(rewrite.path 있으면 교체) 복사
  │    ├─ 원본 헤더 전부 복사
  │    └─ Host 헤더 설정 (기본 포트 80/443는 포트 생략)
  └─ UpstreamClient 생성 후 connect(target.host, target.port)
       → 이후 제어권은 UpstreamClient의 비동기 콜백으로 이동
```

`onEOM` 이후 `GatewayHandler`는 응답을 직접 보내지 않는다.
`UpstreamClient`가 upstream 응답을 받아서 `downstream_`으로 흘려보내며,
upstream 응답의 EOM이 도착하면 그때 Proxygen이 `GatewayHandler::requestComplete()`를 호출해 `delete this`로 정리된다.

---

### 7. UpstreamClient — 비동기 HTTP 프록시

`UpstreamClient`는 `HTTPConnector::Callback`과 `HTTPTransactionHandler`를 동시에 구현한다.

```
connect(addr)
  └─ HTTPConnector::connect(evb_, addr, timeout=5s)
       └─ folly::AsyncSocket으로 비동기 TCP 연결 시작
            (같은 EventBase 스레드에서 연결 완료 콜백 대기)

connectSuccess(session)           ← TCP 연결 성공
  └─ txn_ = session->newTransaction(this)
       ├─ txn_->sendHeaders(*upstreamReq_)   ← 요청 헤더 전송
       ├─ txn_->sendBody(requestBody_)        ← 바디 전송 (있으면)
       └─ txn_->sendEOM()                     ← 요청 완료 신호

connectError(ex)                  ← TCP 연결 실패
  └─ downstream_->sendWithEOM(502) → delete this

onHeadersComplete(msg)            ← upstream 응답 헤더 수신
  └─ downstream_->sendHeaders(*msg)  ← 클라이언트로 그대로 전달

onBody(chain)                     ← upstream 응답 바디 청크 수신
  └─ downstream_->sendBody(chain)    ← 클라이언트로 스트리밍

onEOM()                           ← upstream 응답 완료
  └─ downstream_->sendEOM()
       └─ Proxygen이 GatewayHandler::requestComplete() 호출

detachTransaction()               ← HTTPTransaction 소멸 직전 (항상 마지막)
  └─ delete this
```

upstream 응답은 버퍼링 없이 청크 단위로 클라이언트에 즉시 스트리밍된다.

---

### 8. 객체 수명 요약

| 객체 | 생성 시점 | 소멸 시점 |
|------|-----------|-----------|
| `HeaderFilter` | `HandlerFactory::onRequest` | `requestComplete` or `onError` 후 Proxygen이 정리 |
| `AuthFilter` | 동일 | 동일 |
| `RateLimitFilter` | 동일 | 동일 |
| `GatewayHandler` | 동일 | `requestComplete()` → `delete this` |
| `UpstreamClient` | `GatewayHandler::onEOM` | `detachTransaction()` → `delete this` |

모든 객체는 동일한 I/O 스레드의 EventBase 위에서 생성·소멸되므로 별도의 뮤텍스가 필요 없다.
