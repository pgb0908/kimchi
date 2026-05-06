# Router

**개요**

매칭된 트래픽을 전달할 백엔드 대상을 지정하고, 전달 시 적용할 변환 규칙(경로 재작성, 헤더 수정 등)을 정의합니다.

**필수 필드**

필드명 | 필수 | 설명
---|---|---
apiVersion | Yes | 리소스 버전
kind | Yes | 리소스 종류 (Router)
metadata.name | Yes | 라우터 이름
spec.targetRef.name | Yes | 대상 Listener 이름
spec.rules | Yes | 매칭 규칙 목록
spec.config.destinations | Yes | 전달 대상 목록

**타입별 가이드**

타입 | 주요 필드 | 사용 시나리오
---|---|---
단일 목적지 | destinations[0], weight=100 | 일반 라우팅
다중 목적지 | destinations[], weight 합=100 | 카나리아/분할 배포

**스키마**

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "required": ["apiVersion", "kind", "metadata", "spec"],
  "properties": {
    "apiVersion": { "type": "string", "const": "iip.gateway/v1alpha1" },
    "kind": { "type": "string", "const": "Router" },
    "metadata": {
      "type": "object",
      "required": ["name"],
      "properties": {
        "name": { "type": "string" }
      }
    },
    "spec": {
      "type": "object",
      "required": ["targetRef", "rules", "config"],
      "properties": {
        "targetRef": {
          "type": "object",
          "required": ["name"],
          "properties": {
            "kind": { "type": "string", "default": "Listener" },
            "name": { "type": "string" }
          }
        },
        "rules": {
          "type": "array",
          "items": {
            "type": "object",
            "required": ["path"],
            "properties": {
              "path": { "type": "string" },
              "methods": {
                "type": "array",
                "items": { "type": "string", "enum": ["GET", "POST", "PUT", "DELETE", "PATCH", "OPTIONS", "HEAD"] }
              },
              "headers": { "type": "object" }
            }
          }
        },
        "config": {
          "type": "object",
          "required": ["destinations"],
          "properties": {
            "destinations": {
              "type": "array",
              "items": {
                "type": "object",
                "required": ["destinationRef"],
                "properties": {
                  "destinationRef": {
                    "type": "object",
                    "required": ["name"],
                    "properties": {
                      "kind": { "type": "string", "default": "Service" },
                      "name": { "type": "string" },
                      "namespace": { "type": "string" }
                    }
                  },
                  "weight": { "type": "integer", "minimum": 0, "maximum": 100, "default": 100 },
                  "rewrite": {
                    "type": "object",
                    "properties": {
                      "path": { "type": "string" },
                      "host": { "type": "string" }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
```

**예시**

```json
{
  "apiVersion": "iip.gateway/v1alpha1",
  "kind": "Router",
  "metadata": {
    "name": "route-to-orders"
  },
  "spec": {
    "targetRef": {
      "kind": "Listener",
      "name": "ecommerce-gateway-listener"
    },
    "rules": [
      {
        "path": "/api/orders(/.*)",
        "methods": ["GET", "POST"],
        "headers": {
          "x-client-type": "mobile"
        }
      }
    ],
    "config": {
      "destinations": [
        {
          "destinationRef": {
            "kind": "Service",
            "name": "orders-v1-svc"
          },
          "weight": 90,
          "rewrite": {
            "path": "/v1/orders",
            "host": "orders.internal.svc"
          }
        },
        {
          "destinationRef": {
            "kind": "Service",
            "name": "orders-v2-svc"
          },
          "weight": 10,
          "rewrite": {
            "path": "/v2/orders"
          }
        }
      ]
    }
  }
}
```
