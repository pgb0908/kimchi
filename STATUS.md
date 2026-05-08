# 구현 현황

마지막 업데이트: 2026-05-08

---

## 요청 처리 흐름 대비 현황

```
Client
  → Listener                    ✅ 포트 바인딩 (TLS 미구현)
  → Gateway 전역 정책            ⬜ AccessLogConfig 모델만 존재, 실제 적용 없음
  → Router 매칭                  ✅ 시작 시 regex pre-compile, path+method 매칭
  → Policy 체인 (HeaderFilter    ✅ x-request-id / x-trace-id 주입
              → JwtPolicy        ✅ RS256/JWKS 검증 (x5c 방식)
              → RateLimitFilter) ⬜ 인터페이스만 존재, NullStore (항상 허용)
  → Service 선택 및 upstream     🔶 프록시 동작, 단 LB는 첫 번째 타겟만 사용
  → Access Log / Metrics / Trace ❌ 미구현
```

---

## 완료

| 영역 | 내용 |
|------|------|
| Config 모델 | Listener, Service, Router, Gateway, Policy (JWT) |
| Config 로더 | 디렉토리 JSON 로드, 5가지 kind 파싱 |
| 서버 인프라 | GatewayServer (data plane), AdminServer |
| Admin API | `GET /healthz`, `POST /config/reload` |
| Router 매칭 | GatewayHandlerFactory에서 시작 시 regex pre-compile, 미매칭 즉시 404 |
| Upstream 프록시 | UpstreamClient — Proxygen HTTP 커넥터, 응답 스트리밍 |
| 필터 체인 기반 | FilterBase, HeaderFilter, RateLimitFilter(NullStore) |
| Policy 시스템 | PolicyEngine.buildChain(), order 기반 정렬 |
| JWT 인증 | JwtPolicy — RS256/JWKS, kid 지원, issuer/audience 검증 |
| JWKS 캐시 | JwksCache — 시작 시 blocking fetch, 실패 시 FATAL |
| 릴리즈 빌드 | `scripts/build_release.sh` — `dist/kimchi` 생성 |

---

## 미완료 / 스텁

### 1. Rate Limit — Redis 백엔드
- `RateLimitStore` 인터페이스 정의됨
- `NullRateLimitStore`: 항상 통과 (stub)
- 미구현: Redis 기반 quota 차감, tenant/route 기준 제한

### 2. 로드 밸런싱
- 현재: destinations 배열의 첫 번째 타겟만 사용
- 미구현: weight 기반, ROUND_ROBIN 등 실제 LB 전략

### 3. TLS 종료
- `TlsConfig` 모델 존재, `GatewayServer`에서 경고 로그만 출력
- 미구현: Proxygen/wangle TLS 설정 연동

### 4. JWKS — HTTPS URI 지원
- 현재: HTTP only (POSIX socket)
- 미구현: wangle/fizz TLS 클라이언트로 교체

### 5. JWKS — 자동 갱신
- 현재: 시작 시 1회 fetch, immutable
- 미구현: `cacheTtlSeconds` 기반 background thread + `folly::RWSpinLock` swap

### 6. JWT — n/e 파라미터 방식
- 현재: `x5c` 클레임 방식만 지원
- 미구현: RSA 모듈러스(n) + 지수(e) → OpenSSL BigNum → PEM 변환

### 7. Service 고급 기능
모델은 존재하나 GatewayHandler에서 미사용:
- Health check
- Retry (retryOn, numRetries, backoff)
- Circuit breaker
- Timeout (connect / read / send)
- Upstream TLS

### 8. 관측성
- **Access Log**: `AccessLogConfig` 모델 존재, 실제 로깅 없음
- **Metrics**: 미구현 (Gateway spec에 port 9090 계획)
- **Tracing**: 미구현 (OpenTelemetry 계획)

### 9. Admin API 확장
- 현재: 파일 기반 config reload만 지원
- 미구현: 개별 리소스 CRUD (Control Plane push 방식)
- 미구현: config reload 시 JWKS 재fetch

### 10. Policy 추가 타입
| order | 타입 | 상태 |
|-------|------|------|
| 5 | policy-security (JWT) | ✅ RS256/x5c 구현 |
| 5 | policy-security (IP filter, CORS) | ❌ 미구현 |
| 10 | policy-traffic (Rate Limit) | ⬜ NullStore stub |
| 12 | policy-enhance (캐싱) | ❌ 미구현 |
| 15 | policy-transform (헤더/바디 변환) | ❌ 미구현 |

---

## 우선순위 제안

```
다음 단계 후보 (데이터 플레인 완성도 기준)
  1. Rate Limit Redis 연동          — 실제 트래픽 제어 가능해짐
  2. JWKS n/e 파라미터 지원          — Auth0 등 x5c 없는 IdP 호환
  3. 로드 밸런싱 (weighted/RR)       — 다중 upstream 실용화
  4. JWKS 자동 갱신                  — 운영 안정성

안정성 단계
  5. Retry / Circuit breaker / Timeout
  6. TLS 종료

관측성 단계
  7. Access Log → Metrics → Tracing
```
