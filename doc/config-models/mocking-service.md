# MockingService

백엔드 서비스 없이 API 응답을 시뮬레이션하기 위한 리소스다.
개발/테스트 환경에서 upstream 없이 고정 응답, 지연, 오류율을 설정할 수 있다.

## 리소스 구조

```json
{
  "apiVersion": "iip.gateway/v1alpha1",
  "kind": "MockingService",
  "metadata": {
    "name": "<이름>",
    "revision": "<revision-id>"
  },
  "spec": {
    "stub_payload": {},
    "mock_rules": [],
    "random_delay_range": {
      "min": 0,
      "max": 0
    },
    "error_simulation_rate": 0.0
  }
}
```

## spec 필드

| 필드 | 타입 | 필수 | 설명 |
|------|------|------|------|
| `stub_payload` | object \| string | 선택 | 모든 요청에 반환할 고정 응답 바디 |
| `mock_rules` | array | 선택 | 조건별 응답 분기 규칙 목록 |
| `random_delay_range` | object | 선택 | 응답 지연 시뮬레이션 범위 (ms) |
| `random_delay_range.min` | integer | 선택 | 최소 지연 (ms, 기본값 0) |
| `random_delay_range.max` | integer | 선택 | 최대 지연 (ms) |
| `error_simulation_rate` | number | 선택 | 오류 응답 발생 확률 (0.0~1.0) |

## JSON 스키마

```json
{
  "type": "object",
  "properties": {
    "apiVersion": { "type": "string", "const": "iip.gateway/v1alpha1" },
    "kind":       { "type": "string", "const": "MockingService" },
    "metadata": {
      "type": "object",
      "required": ["name"],
      "properties": {
        "name":     { "type": "string" },
        "revision": { "type": "string" }
      }
    },
    "spec": {
      "type": "object",
      "properties": {
        "stub_payload": { "type": ["object", "string"] },
        "mock_rules": {
          "type": "array",
          "items": { "type": "object" }
        },
        "random_delay_range": {
          "type": "object",
          "properties": {
            "min": { "type": "integer", "minimum": 0 },
            "max": { "type": "integer", "minimum": 0 }
          }
        },
        "error_simulation_rate": {
          "type": "number",
          "minimum": 0.0,
          "maximum": 1.0
        }
      }
    }
  },
  "required": ["apiVersion", "kind", "metadata", "spec"]
}
```

## 예시

### 고정 응답 + 지연 + 오류 시뮬레이션

```json
{
  "apiVersion": "iip.gateway/v1alpha1",
  "kind": "MockingService",
  "metadata": {
    "name": "orders-mock",
    "revision": "local-dev-001"
  },
  "spec": {
    "stub_payload": {
      "message": "This is a mocked response",
      "status": "success"
    },
    "random_delay_range": {
      "min": 100,
      "max": 500
    },
    "error_simulation_rate": 0.05
  }
}
```
