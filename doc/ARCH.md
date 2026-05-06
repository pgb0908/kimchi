http/tcp ingress gateway

- 이 프로젝틑는 ingress-gateway의 데이터 플레인 구현
- control plane은 별도 시스템으로 존재

주요 처리 업무
- config 로드 후 부팅
    - admin을 통해 특정 위치 파일 시스템의 config 로드
    - 혹은 admin api를 통해 config를 동적으로 적용
- Listener, Router, Service, Policy 리소스 로드
- 로그, 메트릭, 트레이싱 기록

data plane 요청 처리 흐름
```text
Client
  -> Listener
  -> Gateway 전역 정책
  -> Router 매칭
  -> policy 수행
  -> Service 선택 및 upstream proxy
  -> Access Log / Metrics / Tracing 기록
```


구조도
```text
┌───────────────────────────────────────────────────────────┐
│                    ingress-gw Process                     │
│                                                           │
│  ┌──────────────────────┐          ┌────────────────────┐  │
│  │    네트워크 data plane │          │     Admin API      │  │
│  │    (예제는 http 포트만) │          │(배포 config, 관리용) │  │
│  │                      │          │                    │  │
│  │   :18000 / :18443    │          │  :18001 / :18444   │  │
│  └──────────┬───────────┘          └─────────┬──────────┘  │
│             │                                │             │
│             └───────────────┬────────────────┘             │
│                             │                              │
│              ┌──────────────▼──────────────┐               │
│              │        현재 적용된 Config     │               │
│              │  (Admin API로 수신한 최신     │               │
│              │   revision 번들 내용)        │               │
│              └─────────────────────────────┘               │
└─────────────────────────────────────────────────────────────┘
                              ▲
                              │ config 전달
                              │ (디렉토리 로드 또는 API push)
               ┌──────────────┴──────────────┐
               │        Control Plane        │
               │  (revision 버전 관리,        │
               │   활성화/롤백 결정)           │
               └─────────────────────────────┘
```
