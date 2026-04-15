# Proxygen Conan Packaging Notes

## 목표

`recipes/` 아래의 커스텀 Conan 레시피를 사용해서 다음 Facebook 네트워크 스택을 소스에서 직접 빌드한다.

- `folly`
- `fizz`
- `wangle`
- `mvfst`
- `proxygen`

최종 목적은 프로젝트에서 `find_package(proxygen)`로 `proxygen`을 링크해서 사용하는 것이다.

## 왜 직접 레시피가 필요한가

`proxygen`과 그 의존성 스택은 ConanCenter에서 바로 가져다 쓸 수 있는 형태로 정리되어 있지 않다. 특히 `proxygen`은 다음 특징 때문에 Conan 패키징 난이도가 높다.

- 의존성 체인이 깊다: `folly -> fizz -> wangle -> mvfst -> proxygen`
- 각 프로젝트가 Conan이 아니라 upstream CMake 설치 구조를 기준으로 설계되어 있다
- 내부 CMake 타깃이 매우 세분화되어 있다
  - 예: `Folly::folly_range`, `wangle::wangle_acceptor_acceptor_core`, `mvfst::mvfst_codec_types`
- HTTP/3까지 사용하려면 `mvfst`와 QUIC 관련 타깃도 살아 있어야 한다

즉, 보통의 단순 Conan 패키지처럼 `cpp_info.libs = ["foo"]`만 맞추면 끝나는 구조가 아니다.

## 현재 빌드 전략

현재는 `recipes/` 아래에 각 라이브러리별 레시피를 두고 `conan create`로 순차 빌드하는 방식을 사용한다.

권장 빌드 순서:

1. `folly`
2. `fizz`
3. `wangle`
4. `mvfst`
5. `proxygen`

예시:

```bash
./conan/bin/conan export recipes/folly
./conan/bin/conan export recipes/fizz
./conan/bin/conan export recipes/wangle
./conan/bin/conan export recipes/mvfst
./conan/bin/conan export recipes/proxygen

./conan/bin/conan create recipes/folly -pr:h=conan/profile -pr:b=conan/profile -b missing
./conan/bin/conan create recipes/fizz -pr:h=conan/profile -pr:b=conan/profile -b missing
./conan/bin/conan create recipes/wangle -pr:h=conan/profile -pr:b=conan/profile -b missing
./conan/bin/conan create recipes/mvfst -pr:h=conan/profile -pr:b=conan/profile -b missing
./conan/bin/conan create recipes/proxygen -pr:h=conan/profile -pr:b=conan/profile -b missing
```

## 레시피에서 하고 있는 일

각 레시피는 대체로 다음 역할을 수행한다.

### 1. 업스트림 소스 다운로드

GitHub tag tarball을 직접 가져와서 빌드한다.

### 2. 업스트림 CMake 패치

upstream 프로젝트들이 기대하는 패키지명과 Conan이 제공하는 패키지명이 다른 부분을 레시피에서 맞춘다.

대표 예시:

- `find_package(Glog REQUIRED)` -> `find_package(glog CONFIG REQUIRED)`
- `find_package(Gflags REQUIRED)` -> `find_package(gflags CONFIG REQUIRED)`
- `find_package(Sodium REQUIRED)` -> `find_package(libsodium CONFIG REQUIRED)`
- `find_package(Zstd REQUIRED)` -> `find_package(zstd CONFIG REQUIRED)`

### 3. Conan/CMake generated metadata 패치

일부 패키지는 Conan이 생성한 CMake 데이터 파일이 upstream 기대와 맞지 않아서 추가 패치가 필요했다.

예:

- `fmt` include 경로 보정
- `glog`의 빈 define 처리 보정

### 4. 패키지 export 정보 정의

`package_info()`에서 CMake 타깃 이름과 라이브러리 이름을 consumer가 찾을 수 있게 정의한다.

## 지금까지 겪은 핵심 문제

### 1. 타깃 이름 체계 불일치

가장 큰 문제는 upstream CMake가 기대하는 타깃 이름과 Conan wrapper가 만드는 타깃 이름이 다르다는 점이었다.

예:

- upstream 기대: `Folly::folly_range`
- Conan wrapper 단순화: `folly::folly`

그래서 다음 같은 에러가 반복되었다.

```text
Target "fizz" links to:
  Folly::folly_range
but the target was not found.
```

이 문제는 `Folly::*` 같은 세분화된 컴포넌트 타깃을 Conan이 자동으로 재현하지 못하기 때문에 발생한다.

### 2. native config와 Conan wrapper를 섞어서 사용한 문제

중간에 두 전략이 섞이면서 문제가 반복되었다.

- 전략 A: upstream가 설치한 `lib/cmake/...`의 native config 사용
- 전략 B: Conan `CMakeDeps`가 생성한 wrapper config 사용

이 둘이 섞이면 다음 같은 현상이 생긴다.

- 어떤 단계에서는 `Folly::...`를 기대
- 다른 단계에서는 `folly::folly`만 존재
- `find_package()`가 어느 파일을 먼저 잡느냐에 따라 결과가 달라짐

즉, 에러가 계속 돌고 도는 가장 큰 이유는 개별 라이브러리 문제가 아니라 빌드 구조가 일관되지 않았기 때문이다.

### 3. 실제 설치된 라이브러리 이름과 `package_info()` 불일치

`proxygen`은 실제로 다음 같은 라이브러리를 설치한다.

- `libproxygen.a`
- `libproxygen_httpserver.a`

그런데 초기 `package_info()`는 다음처럼 잘못 정의되어 있었다.

- `proxygenlib`
- `proxygenhttpserver`

이 때문에 consumer 단계에서 다음 같은 에러가 발생했다.

```text
Library 'proxygenhttpserver' not found in package.
```

즉 CMake 타깃명은 맞더라도, 그 타깃이 실제로 가리키는 `.a` 이름이 틀리면 링크 단계에서 깨진다.

### 4. 내부 패키지 빌드와 최종 앱 소비 방식이 다름

스택 내부 빌드에서는 upstream native config가 더 잘 맞지만, 최종 앱에서는 현재 `cmake-conan` provider가 generator 폴더를 우선 탐색한다.

이 때문에 다음 충돌이 생겼다.

- 내부 패키지 빌드는 `lib/cmake/<pkg>` native config를 원함
- 앱 `find_package(proxygen)`는 generator 폴더 안의 Conan wrapper를 원함

즉, 패키지 빌드 전략과 consumer 전략을 같은 방식으로 처리하기 어려웠다.

### 5. transitive dependency와 `find_dependency()` 체인 문제

`proxygen` wrapper가 `find_dependency(wangle)`를 호출하면, `wangle`도 generator 폴더에서 찾아져야 한다. 하지만 `wangle`, `fizz`, `folly`, `mvfst`가 `cmake_find_mode = none`이면 generator 폴더에 대응 파일이 생기지 않는다.

그래서 다음 같은 에러가 발생했다.

```text
Could NOT find wangle (missing: wangle_DIR)
```

즉 상위 패키지 하나만 wrapper를 만들어도 해결되지 않고, 그 dependency chain 전체의 find strategy가 맞아야 한다.

## 현재 이해해야 할 핵심

지금 문제는 단순히 의존성 그래프가 복잡해서가 아니다. 실제 문제는 다음 세 가지다.

- upstream CMake target naming
- Conan package metadata naming
- CMake `find_package()` resolution order

의존성 그래프에서 `fizz -> zlib`와 `folly -> zlib`가 동시에 보이는 것은 이상 현상이 아니다. 그 그래프는 "누가 누구를 직접 requirement로 선언했는가"를 보여주기 때문에, 직접 의존성과 전이 의존성이 함께 나타날 수 있다.

즉 그래프 자체는 대체로 정상이고, 실제 난제는 타깃 해석과 consumer metadata다.

## 현재까지 정리된 방향

현 시점에서 확인된 방향은 다음과 같다.

- HTTP/3를 사용할 계획이면 `mvfst`를 포함하는 방향으로 가야 한다
- `proxygen`만 빌드하는 문제가 아니라 Facebook 스택 전체를 맞춰야 한다
- 스택 내부 빌드용 전략과 최종 consumer 전략을 분리해서 생각해야 한다
- `package_info()`는 실제 설치된 라이브러리 이름과 exported target 구조를 기준으로 작성해야 한다

## 권장 정리 방향

구조를 안정화하려면 다음 중 하나를 선택해야 한다.

### 방향 1. native config 중심

- `folly/fizz/wangle/mvfst/proxygen` 내부 빌드는 upstream native config만 사용
- Conan wrapper는 최소한만 유지
- HTTP/3 포함한 upstream 구조에는 이 방식이 더 자연스럽다

### 방향 2. Conan wrapper 중심

- `Folly::*`, `wangle::*`, `mvfst::*`, `proxygen::*`의 세분화된 컴포넌트를 Conan `package_info()`로 재현
- 구현량이 매우 많고 유지보수 비용이 큼

현재 상황상 방향 1이 더 현실적이지만, 최종 앱이 `cmake-conan` provider를 사용하고 있기 때문에 consumer 쪽 연결은 별도 보완이 필요하다.

## 요약

이 작업은 "Conan으로 오픈소스 하나를 추가한다" 수준이 아니라, Facebook 네트워크 스택 전체를 Conan 생태계에 맞게 번역하는 작업에 가깝다.

문제가 반복된 이유는 개별 에러 하나하나보다 다음 구조 충돌 때문이다.

- upstream native CMake 생태계
- Conan CMakeDeps wrapper 생태계
- 최종 프로젝트의 `cmake-conan` 소비 방식

따라서 이후 작업은 에러 메시지를 하나씩 막는 방식보다, 어떤 계층에서 어떤 방식으로 `find_package()`를 해석할지 먼저 고정하는 것이 중요하다.
