---
name: build-ubuntu20
description: Ubuntu 20.04 Docker 빌드를 백그라운드로 실행하고 결과를 모니터링한다. "ubuntu20 빌드", "build-ubuntu20", "/build-ubuntu20" 언급 시 사용.
---

모든 응답은 한국어로 한다.

1. `scripts/build_ubuntu20.sh`를 `run_in_background: true`로 실행한다. 로그는 `/tmp/kimchi-ubuntu20-build.log`에 tee로 저장한다:
   ```
   bash scripts/build_ubuntu20.sh 2>&1 | tee /tmp/kimchi-ubuntu20-build.log
   ```

2. 백그라운드 실행 직후 Monitor 도구로 로그를 감시한다:
   - 감시 명령: `tail -f /tmp/kimchi-ubuntu20-build.log | grep --line-buffered -E "BUILD_FAILED|requires the following|Binary ready|Built target kimchi|=== |^ERROR"`
   - timeout: 3600000ms
   - 에러 발생 시 즉시 원인을 분석하고 Dockerfile.build를 수정한 뒤 다시 빌드한다.
   - "Binary ready" 또는 "Built target kimchi" 메시지가 나오면 성공으로 보고한다.

3. 에러가 반복될 경우 수정 → 재빌드를 성공할 때까지 반복한다.
