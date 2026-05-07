# Policy System

Policy 시스템은 kimchi 데이터 플레인에서 요청 처리 파이프라인을 설정 기반으로 구성하는 메커니즘이다. 기존에 하드코딩된 필터 체인을 대체하여, `Policy` 리소스와 `Router.spec.policies` 참조만으로 라우트별 인증·인가·트래픽 제어 동작을 정의할 수 있다.

---

## 아키텍처

```
GatewayHandlerFactory::onRequest(msg)
    │
    ├─ Router 매칭 (path regex + method, 시작 시 pre-compile)
    │       │
    │       └─ 매칭 실패 → NotFoundHandler (404)
    │
    └─ PolicyEngine::buildChain(matchedRouter, GatewayHandler)
            │
            ▼
    HeaderFilter                  ← 항상 최외곽 (request-id, trace-id 주입)
        │
    JwtPolicy (order=5)           ← Router.policies에 JWT 정책 있을 때만
        │
    RateLimitFilter (order=10)    ← 항상 포함 (v1: NullStore stub)
        │
    GatewayHandler                ← pre-matched RouterConfig으로 upstream 프록시
```

### 설계 결정

| 항목 | 결정 | 이유 |
|------|------|------|
| 하드코딩 체인 → PolicyEngine | 교체 | 정책 추가 시 코드 변경 없이 config만 수정 |
| Router 매칭 위치 | Factory | Handler는 upstream 프록시에만 집중 |
| 미매칭 라우트 처리 | 즉시 404 | Fail-closed, 불필요한 필터 실행 없음 |
| Policy 부착 단위 | Router별 | 경로마다 다른 정책 필요 (예: /health는 인증 없이) |
| Regex pre-compile | 시작 시 | per-request regex 생성 비용 제거 |

---

## 리소스 정의

### Policy 리소스

```json
{
  "apiVersion": "kimchi/v1",
  "kind": "Policy",
  "metadata": {
    "name": "policy-security"
  },
  "spec": {
    "order": 5,
    "security": {
      "jwt": {
        "jwksUri": "http://idp.internal/.well-known/jwks.json",
        "issuer": "https://idp.example.com",
        "audience": "kimchi-gateway",
        "cacheTtlSeconds": 300
      }
    }
  }
}
```

#### spec 필드

| 필드 | 타입 | 설명 |
|------|------|------|
| `order` | int | 실행 순서. 낮을수록 외곽 (먼저 실행). 5=security, 10=traffic, 15=transform |
| `security.jwt.jwksUri` | string | JWKS JSON을 제공하는 엔드포인트 URI |
| `security.jwt.issuer` | string | JWT `iss` 클레임 검증값. 비어있으면 검증 생략 |
| `security.jwt.audience` | string | JWT `aud` 클레임 검증값. 비어있으면 검증 생략 |
| `security.jwt.cacheTtlSeconds` | int | JWKS 캐시 TTL (v1에서 문서화 목적, 자동 갱신 미구현) |

### Router 리소스 (policies 필드 추가)

```json
{
  "apiVersion": "kimchi/v1",
  "kind": "Router",
  "metadata": { "name": "api-router" },
  "spec": {
    "targetRef": { "name": "default-listener" },
    "policies": ["policy-security"],
    "rules": [
      { "path": "/api/.*", "methods": ["GET", "POST"] }
    ],
    "config": {
      "destinations": [
        {
          "destinationRef": { "kind": "Service", "name": "backend-svc" },
          "weight": 100
        }
      ]
    }
  }
}
```

`policies` 배열은 Policy 리소스 이름의 목록이다. PolicyEngine이 `spec.order` 기준으로 정렬하여 체인을 구성하므로, 배열 순서는 무관하다.

---

## JWT 검증 흐름

```
요청 수신
    │
    ├─ Authorization 헤더 없음 → 401
    ├─ Bearer 형식 아님 → 401
    │
    ▼
jwt::decode(token)
    │
    ├─ "kid" 헤더 있음 → JWKS에서 해당 kid JWK 조회
    │       └─ kid 없음 → 401
    │
    └─ "kid" 없음 → JWKS 첫 번째 키 사용
    │
    ▼
jwt::verify()
    .allow_algorithm(rs256(pub_key_pem))
    [.with_issuer(issuer)]      ← config.issuer 설정된 경우
    [.with_audience(audience)]  ← config.audience 설정된 경우
    .verify(decoded)
    │
    ├─ 실패 (만료, 서명불일치, issuer/audience 불일치) → 401
    │
    └─ 성공
            │
            ▼
        upstream 헤더 주입:
            x-jwt-subject: <sub 클레임>
            x-jwt-issuer:  <iss 클레임>
            x-auth-method: "jwt"
            Authorization: (제거)  ← upstream에 원본 토큰 전달 안 함
```

### 지원 알고리즘

v1에서는 **RS256만 지원**한다. `jwt-cpp` 라이브러리의 `rs256` 알고리즘을 사용하며, OpenSSL을 통해 서명을 검증한다 (Proxygen이 이미 OpenSSL을 링크).

ES256, HS256 등 다른 알고리즘은 `jwt_policy.cpp`에서 알고리즘별 분기를 추가하여 확장 가능하다.

---

## JWKS 캐시

### 동작

```
main() 시작
    │
    ├─ ConfigStore에서 spec.security.jwt가 있는 Policy 열거
    │
    └─ 각 Policy에 대해 JwksCache::fetch(jwtConfig)
            │
            ├─ HTTP GET jwksUri (POSIX socket, 10초 timeout)
            ├─ HTTP 200 응답 확인
            └─ JWKS JSON 저장 → shared_ptr<JwksCache> 반환
                    │
                    └─ 실패 시 LOG(FATAL) → 프로세스 종료
                       (JWKS 없이 트래픽을 받으면 안 됨)
    │
    └─ GatewayServer(listeners, store, jwksCaches) 시작
```

### v1 제약사항

| 항목 | 현황 | 향후 작업 |
|------|------|---------|
| HTTPS JWKS URI | **미지원** | wangle/fizz로 TLS 클라이언트 추가 |
| 자동 갱신 | **미구현** | `cacheTtlSeconds` 기반 background thread |
| 갱신 중 락 | 설계됨 | `folly::RWSpinLock` + `shared_ptr` swap |
| 여러 JWKS 소스 | 지원 | Policy별로 별도 JwksCache 인스턴스 |

---

## PolicyEngine 체인 빌드 알고리즘

```cpp
buildChain(router, terminal):
    1. router.policies 이름 목록으로 store_.policies에서 PolicyConfig 조회
    2. spec.order 오름차순 정렬
    3. terminal ← RateLimitFilter(terminal)    // 항상 내측에 추가
    4. 역순 순회 (높은 order부터):
       - security.jwt 있고 JwksCache 있으면 → JwtPolicy(current)로 wrap
    5. HeaderFilter(current)로 wrap            // 항상 최외곽
    6. return 최외곽 필터
```

`active.rbegin()` → `rend()` 역순 순회로 낮은 order (높은 우선순위)가 더 외곽에 위치하게 된다.

---

## 파일 구조

```
src/
├── policy/
│   ├── jwks_cache.h/.cpp     JWKS fetch 및 캐시 (immutable, v1)
│   ├── jwt_policy.h/.cpp     JWT 검증 Proxygen Filter
│   └── policy_engine.h/.cpp  RouterConfig 기반 필터 체인 빌드
├── handler/
│   ├── gateway_handler_factory.*  Router 매칭 + PolicyEngine 호출
│   └── gateway_handler.*          pre-matched Router 기반 upstream 프록시
└── config/
    └── models.h              PolicyConfig, JwtConfig, RouterConfig.policies 추가
```

---

## 확장 계획

### 다음 Policy 타입

| order | kind | 주요 동작 |
|-------|------|---------|
| 5 | policy-security | ✅ JWT (v1), IP filter, CORS (v2) |
| 10 | policy-traffic | Rate Limit Redis 연동, SLA 차등 |
| 12 | policy-enhance | 응답 캐싱 |
| 15 | policy-transform | 헤더/바디 변환, 마스킹 |

### HTTPS JWKS 지원

`JwksCache::fetch`의 HTTP 클라이언트를 `wangle::SSLContextConfig`를 활용한 TLS 소켓으로 교체한다. 내부 클러스터 IdP는 대부분 plain HTTP를 지원하므로 v1에서는 허용 가능한 제약이다.

### JWKS 자동 갱신

```
시작 시 JwksCache 생성 (현재 v1)
    ↓
별도 std::thread에서 sleep(cacheTtlSeconds / 2)
    ↓
JwksCache::fetch() 재실행
    ↓
folly::RWSpinLock write-lock → shared_ptr 교체
    ↓
IO 스레드는 read-lock으로 기존 캐시 읽기 (블로킹 없음)
```

### Hot reload 대응

현재 `GatewayHandlerFactory`는 시작 시 `ConfigStore`의 내용으로 pre-compile하며, Admin API를 통한 config reload가 factory에 반영되지 않는다. 향후 `SharedConfig`와 연동하여 factory가 atomic하게 교체되는 구조가 필요하다.
