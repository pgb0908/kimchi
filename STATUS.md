# 구현 현황

ARCH.md 및 doc/config-models 기준으로 현재 구현 상태를 정리한다.

## 요청 처리 흐름 대비 현황

```
Client
  → Listener                    ✅ 포트 바인딩 (TLS 미구현)
  → Gateway 전역 정책            ⬜ AccessLog 모델만 존재, 실제 적용 없음
  → Router 매칭                  ❌ GatewayHandler가 stub (무조건 200 OK)
  → policy 수행                  🔶 Filter 체인만 존재, Policy 리소스 미구현
  → Service 선택 및 upstream     ❌ 미구현
  → Access Log / Metrics / Trace ❌ 미구현
```

---

## 완료

| 영역 | 내용 |
|------|------|
| Config 모델 | Listener, Service, Router, Gateway, ConfigStore |
| Config 로더 | 디렉토리 JSON 로드, Admin API를 통한 reload |
| 서버 인프라 | GatewayServer, AdminServer, Listener config 기반 포트 바인딩 |
| Admin API | `GET /healthz`, `POST /config/reload` |
| 필터 체인 | FilterBase, AuthFilter, HeaderFilter, RateLimitFilter (stub) |

---

## 미완료

### 1. Router 매칭 — 핵심
- `GatewayHandler::onEOM`이 stub 상태 (모든 요청에 고정 응답 반환)
- ConfigStore의 RouterConfig를 읽어 path/method 매칭 로직 구현 필요
- RouterRule의 path는 regex 지원 필요 (`/api/orders(/.*)`형태)

### 2. Upstream 프록시 — 핵심
- 매칭된 RouterDestination → ServiceConfig 타겟으로 HTTP 전달
- `rewrite.path` 적용
- Proxygen의 `HTTPConnector` 또는 클라이언트 사용

### 3. Policy 시스템
- ConfigStore에 Policy 리소스 모델 없음
- `spec.order` 기반 실행 순서 엔진 필요
- 구현 필요한 Policy 종류 (doc/config-models 기준):

| Policy | order | 내용 |
|--------|-------|------|
| policy-security | 5 | JWT 검증, IP 필터, CORS |
| policy-traffic | 10 | Rate Limit 고도화, SLA 차등 적용 |
| policy-enhance | 12 | 캐싱 |
| policy-transform | 15 | 헤더/쿼리 조작, 바디 변환, 마스킹 |

### 4. Service 고급 기능
모델은 존재하나 실제 동작 없음:
- Load balancing (ROUND_ROBIN 등)
- Health check
- Retry (retryOn, numRetries, backoff)
- Circuit breaker
- Timeout (connect / read / send)
- Upstream TLS

### 5. Rate Limit Redis 연동
- `RateLimitStore` 인터페이스만 존재, `NullRateLimitStore` (항상 허용) stub
- Redis 기반 구현체 필요 (tenant/route 기준 quota 차감)

### 6. 관측성
- **Access Log**: `AccessLogConfig` 모델 존재, 실제 로깅 없음
- **Metrics**: 미구현 (Gateway spec에 `/metrics`, port 9090 계획)
- **Tracing**: 미구현 (OpenTelemetry 엔드포인트 계획)

### 7. TLS 종료
- `TlsConfig` 모델 존재 (`GatewayServer`에서 경고 로그만 출력)
- Proxygen의 TLS 설정 연동 필요

### 8. Admin API 확장
- 현재: 파일 기반 config reload만 지원
- 미구현: 개별 리소스 CRUD (Control Plane에서 push 방식)

### 9. message-filter
- `doc/Filter.md`에 계획 중으로 명시됨
- 바디 전문 파싱, 메시지 스키마 배포 기반 검증

---

## 우선순위 제안

```
1단계 (데이터 플레인 기본 동작)
  → Router 매칭 + Upstream 프록시

2단계 (Policy 시스템)
  → Policy 모델 + 실행 엔진
  → policy-security (JWT, IP 필터)
  → policy-traffic (Rate Limit Redis 연동)

3단계 (안정성)
  → Retry / Circuit breaker / Health check
  → TLS 종료

4단계 (관측성)
  → Access Log → Metrics → Tracing
```
