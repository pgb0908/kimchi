# Proxygen Conan Packaging Notes

## 목표

`recipes/` 아래의 커스텀 Conan 레시피를 사용해서 다음 Facebook 네트워크 스택을 소스에서 직접 빌드한다.

- `folly`
- `fizz`
- `wangle`
- `mvfst`
- `proxygen`

최종 목적은 프로젝트에서 `find_package(proxygen)`로 `proxygen`을 링크해서 사용하는 것이다.

```cmake
# src/CMakeLists.txt
find_package(proxygen REQUIRED)
add_executable(main main.cpp)
target_link_libraries(main PRIVATE proxygen::proxygen proxygen::proxygenhttpserver)
```

## 왜 직접 레시피가 필요한가

`proxygen`과 그 의존성 스택은 ConanCenter에서 바로 가져다 쓸 수 있는 형태로 정리되어 있지 않다. 특히 `proxygen`은 다음 특징 때문에 Conan 패키징 난이도가 높다.

- 의존성 체인이 깊다: `folly → fizz → wangle → mvfst → proxygen`
- 각 프로젝트가 Conan이 아니라 upstream CMake 설치 구조를 기준으로 설계되어 있다
- 내부 CMake 타깃이 매우 세분화되어 있다
  - 예: `Folly::folly_range`, `wangle::wangle_acceptor_acceptor_core`, `mvfst::mvfst_codec_types`
- HTTP/3까지 사용하려면 `mvfst`와 QUIC 관련 타깃도 살아 있어야 한다

즉, 보통의 단순 Conan 패키지처럼 `cpp_info.libs = ["foo"]`만 맞추면 끝나는 구조가 아니다.

## 현재 빌드 흐름

### 전체 구조

```
CMakeLists.txt
├── conan 자동 설치 (scripts/install_conan.sh)
├── 레시피 자동 export (scripts/export_recipes.sh)
├── cmake-conan provider (cmake/conan_provider.cmake)
│   └── find_package() 가로채기 → conan install --build=missing
└── project(kimchi)
    └── src/CMakeLists.txt → find_package(proxygen)
```

### 동작 순서

1. `cmake -B build -DCMAKE_BUILD_TYPE=Debug`
2. CMakeLists.txt 상단에서 Conan 미설치 시 자동 설치
3. `scripts/export_recipes.sh` 실행 → 5개 레시피를 Conan 캐시에 등록 (빌드 없이 레시피만, 수초 소요)
4. `src/CMakeLists.txt`에서 `find_package(proxygen)` 호출
5. `cmake-conan` provider가 가로채서 `conan install --build=missing` 실행
6. Conan이 의존성 그래프를 해석하고 빠진 패키지를 자동 빌드: `folly → fizz → wangle → mvfst → proxygen`
7. 빌드 결과가 Conan 캐시에 저장되고, CMakeDeps가 generators 폴더에 config 파일 생성
8. `find_package(proxygen)` 성공 → `proxygen::proxygen`, `proxygen::proxygenhttpserver` 타깃 사용 가능
9. `cmake --build build`로 프로젝트 빌드

### 버전 관리

모든 Facebook 스택 라이브러리의 버전은 `recipes/meta_version.txt` 한 곳에서 관리한다.

```
2026.04.13.00
```

각 레시피는 `set_version()`에서 이 파일을 읽어 버전을 설정하고, `requirements()`에서도 `self.version`을 참조해 스택 내 의존성 버전을 일치시킨다. 버전 업그레이드 시 이 파일 하나만 수정하면 된다.

```python
def set_version(self):
    p = os.path.join(os.path.dirname(__file__), "..", "meta_version.txt")
    self.version = open(p).read().strip()

def requirements(self):
    self.requires(f"folly/{self.version}")
```

root `conanfile.py`에서도 동일하게 `recipes/meta_version.txt`를 참조한다.

## 레시피에서 하고 있는 일

### 1. 업스트림 소스 다운로드

GitHub tag tarball을 직접 가져와서 빌드한다. URL도 `self.version`을 사용해 동적 생성.

### 2. 업스트림 CMake 패치 (`_patch_upstream_cmake`)

upstream 프로젝트들이 기대하는 패키지명과 Conan이 제공하는 패키지명이 다른 부분을 `replace_in_file`로 맞춘다.

| upstream 코드 | Conan 패치 결과 | 해당 레시피 |
|---|---|---|
| `find_package(Glog REQUIRED)` | `find_package(glog CONFIG REQUIRED)` | fizz, wangle, mvfst, proxygen |
| `find_package(Gflags REQUIRED)` | `find_package(gflags CONFIG REQUIRED)` | wangle, mvfst, proxygen |
| `find_package(Sodium REQUIRED)` | `find_package(libsodium CONFIG REQUIRED)` | fizz |
| `find_package(Zstd REQUIRED)` | `find_package(zstd CONFIG REQUIRED)` | fizz |
| `find_package(Fizz CONFIG REQUIRED)` | `find_package(fizz CONFIG REQUIRED)` | wangle, proxygen |
| `find_package(Wangle CONFIG REQUIRED)` | `find_package(wangle CONFIG REQUIRED)` | proxygen |

### 3. Conan 생성 메타데이터 패치

일부 패키지는 Conan CMakeDeps가 생성한 데이터 파일 자체에 문제가 있어 추가 패치가 필요하다.

**fmt include 경로 보정 (fizz)**

Conan의 `fmt` 레시피가 `components["_fmt"]`를 사용하면서 `includedirs`를 명시하지 않아 `INTERFACE_INCLUDE_DIRECTORIES`가 빈 값으로 생성된다. `_patch_fmt_data()`가 생성된 `fmt-*-data.cmake`에서 include 경로를 직접 삽입한다.

**glog define 보정 (mvfst, proxygen)**

`glog` 래퍼가 `GFLAGS_DLL_DECLARE_FLAG=`와 `GFLAGS_DLL_DEFINE_FLAG=`를 빈 값으로 define하면서 downstream 빌드에서 경고/에러가 발생한다. `_patch_glog_data()`가 이 빈 define들을 제거한다.

**folly의 `GLOG_USE_GLOG_EXPORT` 보정 (folly)**

folly의 `folly-deps.cmake`가 glog 관련 `GLOG_USE_GLOG_EXPORT` 플래그를 추가하는데, Conan 환경에서는 중복/충돌을 일으키므로 `_patch_upstream_cmake()`에서 제거한다.

### 4. 패키지 export 정보 정의 (`package_info`)

`package_info()`에서 consumer가 `find_package()`로 패키지를 찾을 수 있도록 메타데이터를 정의한다. 이 부분이 가장 핵심적이며, 아래에서 상세히 설명한다.

## 핵심 설계: `cmake_find_mode` 이중 전략

이 프로젝트에서 가장 어려웠던 문제이자, 현재 해결책의 핵심이다.

### 문제 배경: 두 가지 세계의 충돌

Facebook 스택을 Conan으로 패키징할 때, 근본적으로 충돌하는 두 가지 요구사항이 있다.

**요구사항 A — 내부 빌드 (conan create)**

fizz를 빌드할 때, fizz의 upstream CMakeLists.txt는 folly의 세분화된 컴포넌트 타깃을 기대한다.

```cmake
# fizz의 upstream CMakeLists.txt가 기대하는 것
target_link_libraries(fizz PUBLIC Folly::folly_range)
```

이 타깃은 folly가 `cmake install`할 때 생성하는 **native config** (`lib/cmake/folly/folly-targets.cmake`)에 정의되어 있다. Conan CMakeDeps 래퍼는 이 세분화된 타깃을 모르고 `Folly::folly`만 생성하므로, 래퍼를 사용하면 내부 빌드가 깨진다.

**요구사항 B — Consumer 빌드 (최종 앱)**

최종 앱에서 `find_package(proxygen)`을 호출할 때, cmake-conan provider는 generators 폴더에서 config 파일을 찾는다. `cmake_find_mode = "none"`이면 CMakeDeps가 아무 파일도 생성하지 않으므로 `find_package`가 실패한다.

```
Could NOT find proxygen (missing: proxygen_DIR)
```

### 시도하고 실패한 접근들

#### 접근 1: `cmake_find_mode = "none"` (전체 적용)

```python
self.cpp_info.set_property("cmake_find_mode", "none")
self.cpp_info.builddirs = ["lib/cmake/folly"]
```

- 내부 빌드: 성공 (native config 사용, 컴포넌트 타깃 사용 가능)
- Consumer 빌드: **실패** — CMakeDeps가 래퍼를 생성하지 않음. cmake-conan provider는 generators 폴더만 탐색하므로 패키지를 찾지 못함. `builddirs`가 `CMAKE_PREFIX_PATH`에 추가되긴 하지만, provider가 `conan_toolchain.cmake`를 include하지 않아 해당 경로가 실제 탐색에 반영되지 않음.

#### 접근 2: `cmake_find_mode = "none"` + provider에서 toolchain include

```cmake
# conan_provider.cmake에 추가
include("${_conan_generators_folder}/conan_toolchain.cmake")
```

- Consumer 빌드: proxygen native config를 찾긴 하지만, native config 내부에서 `find_dependency(Sodium)` 호출 시 실패. Conan 패키지명은 `libsodium`이므로 `sodium-config.cmake`가 존재하지 않음.
- 즉, native config가 사용하는 upstream 패키지명(`Sodium`, `Glog` 등)과 Conan 패키지명(`libsodium`, `glog` 등)이 불일치하는 곳마다 연쇄 실패 발생.

#### 접근 3: `cmake_find_mode = "both"` (전체 적용)

```python
self.cpp_info.set_property("cmake_find_mode", "both")
```

- Consumer 빌드: 성공 (CMakeDeps 래퍼가 generators 폴더에 생성됨)
- 내부 빌드: **실패** — CMakeDeps 래퍼가 generators 폴더에 생성되고, `find_package(folly)`가 래퍼를 먼저 찾음. 래퍼는 `Folly::folly`만 정의하므로 `Folly::folly_range`을 찾을 수 없음.

```
Target "fizz" links to:
  Folly::folly_range
but the target was not found.
```

### 현재 해결책: `package_info`는 `"both"`, `generate`에서 내부 의존성만 `"none"` 오버라이드

핵심 아이디어: **같은 패키지라도 누가 소비하느냐에 따라 find_mode를 다르게 적용**한다.

**`package_info()`** — 기본값으로 `cmake_find_mode = "both"` 설정:

```python
def package_info(self):
    self.cpp_info.set_property("cmake_find_mode", "both")
    self.cpp_info.set_property("cmake_file_name", "folly")
    self.cpp_info.set_property("cmake_target_name", "Folly::folly")
    self.cpp_info.builddirs = ["lib/cmake/folly"]
    self.cpp_info.libs = ["folly"]
```

이 설정은 consumer(최종 앱)에서 사용된다. CMakeDeps가 래퍼를 생성하므로 cmake-conan provider가 generators 폴더에서 정상적으로 패키지를 찾는다.

**`generate()`** — 내부 빌드 시 Facebook 의존성을 `"none"`으로 오버라이드:

```python
def generate(self):
    deps = CMakeDeps(self)
    deps.set_property("folly", "cmake_find_mode", "none")
    deps.generate()
```

`conan create`로 fizz를 빌드할 때 실행되는 코드이다. folly에 대해 CMakeDeps 래퍼를 생성하지 않으므로, `find_package(folly)`가 `CMakeToolchain`이 `CMAKE_PREFIX_PATH`에 추가한 native config(`lib/cmake/folly/folly-config.cmake`)를 찾는다. native config는 `Folly::folly_range` 등 모든 컴포넌트 타깃을 제공한다.

#### 각 레시피별 오버라이드 대상

| 레시피 | `generate()`에서 `"none"` 적용 대상 | 이유 |
|---|---|---|
| fizz | folly | `Folly::folly_range` 등 필요 |
| wangle | folly, fizz | `Folly::*`, `fizz::*` 컴포넌트 필요 |
| mvfst | folly, fizz | 동일 |
| proxygen | folly, fizz, wangle, mvfst | 모든 upstream 타깃 필요 |

#### 동작 흐름 요약

```
[conan create fizz]
  generate():  deps.set_property("folly", "cmake_find_mode", "none")
  빌드 시:     find_package(folly) → native config → Folly::folly_range ✓

[cmake-conan provider에서 consumer 빌드]
  conan install: folly의 package_info()에서 cmake_find_mode = "both"
  CMakeDeps:     generators/folly-config.cmake 래퍼 생성 → Folly::folly ✓
  find_package:  generators 폴더에서 래퍼 발견 → 성공 ✓
```

## 겪었던 주요 문제와 해결

### 문제 1: 타깃 이름 체계 불일치

upstream CMake가 기대하는 타깃 이름과 Conan CMakeDeps 래퍼가 만드는 타깃 이름이 다르다.

| upstream 기대 | Conan 래퍼 생성 |
|---|---|
| `Folly::folly_range` | `Folly::folly` |
| `wangle::wangle_acceptor_*` | `wangle::wangle` |
| `mvfst::mvfst_codec_types` | `mvfst::mvfst` |

**해결:** `generate()`에서 `deps.set_property(pkg, "cmake_find_mode", "none")`으로 내부 빌드 시 래퍼 생성을 억제하고, native config를 사용하도록 함.

### 문제 2: native config와 Conan 래퍼 혼용

`cmake_find_mode = "both"`를 전체 적용하면, 래퍼가 generators 폴더에 생성되고 native config보다 먼저 잡힌다. 래퍼에는 컴포넌트 타깃이 없으므로 내부 빌드가 실패한다.

`cmake_find_mode = "none"`을 전체 적용하면 래퍼가 없어서 consumer 빌드가 실패한다.

**해결:** `package_info()` = `"both"` (consumer용), `generate()` = `"none"` 오버라이드 (내부 빌드용)로 분리.

### 문제 3: 패키지명 불일치와 `find_dependency` 체인

native config가 `find_dependency(Sodium)` 등을 호출하지만, Conan 패키지명은 `libsodium`이므로 `sodium-config.cmake`가 존재하지 않는다.

| native config 호출 | Conan 패키지명 | 탐색 파일명 | 매칭 |
|---|---|---|---|
| `find_dependency(Sodium)` | `libsodium` | `sodium-config.cmake` | **불일치** |
| `find_dependency(Glog)` | `glog` | `glog-config.cmake` | 일치 (대소문자 무시) |
| `find_dependency(Gflags)` | `gflags` | `gflags-config.cmake` | 일치 |
| `find_dependency(Fizz)` | `fizz` | `fizz-config.cmake` | 일치 |
| `find_dependency(fmt)` | `fmt` | `fmt-config.cmake` | 일치 |

**해결:** consumer에서는 `cmake_find_mode = "both"` 래퍼를 사용하므로 이 문제가 발생하지 않음. 래퍼가 Conan 패키지명으로 `find_dependency`를 호출하기 때문. 내부 빌드에서는 upstream CMake를 `replace_in_file`로 패치하여 Conan 패키지명을 직접 사용하도록 변경.

### 문제 4: 실제 라이브러리 이름과 `package_info()` 불일치

`proxygen`은 실제로 `libproxygen.a`와 `libproxygen_httpserver.a`를 설치한다. `package_info()`의 `libs`와 `components`가 이 이름과 일치해야 한다.

```python
# proxygen/conanfile.py
def package_info(self):
    self.cpp_info.libs = ["proxygen"]
    self.cpp_info.components["proxygenhttpserver"].libs = ["proxygen_httpserver"]
    self.cpp_info.components["proxygenhttpserver"].requires = ["proxygen"]
    self.cpp_info.components["proxygen"].libs = ["proxygen"]
```

### 문제 5: cmake-conan provider가 `conan_toolchain.cmake`를 include하지 않음

`cmake_find_mode = "none"` 사용 시, `builddirs`를 통해 native config 경로가 `conan_toolchain.cmake`의 `CMAKE_PREFIX_PATH`에 추가된다. 하지만 cmake-conan provider는 이 toolchain 파일을 include하지 않으므로, `CMAKE_PREFIX_PATH`가 실제로 설정되지 않아 native config를 찾지 못한다.

**해결:** consumer에서는 `cmake_find_mode = "both"`를 사용하므로 이 문제를 우회. 래퍼가 generators 폴더에 생성되고, provider는 generators 폴더를 우선 탐색하므로 정상 동작.

### 문제 6: Conan CMakeDeps 버그 (fmt include 경로)

Conan의 `fmt` 레시피가 생성하는 데이터 파일에서 `INTERFACE_INCLUDE_DIRECTORIES`가 빈 값으로 설정된다. `conan_toolchain.cmake`가 `CMAKE_FIND_PACKAGE_PREFER_CONFIG ON`을 설정하므로 `Findfmt.cmake` 모듈 방식도 동작하지 않는다.

**해결:** fizz 레시피의 `_patch_fmt_data()`에서 생성된 데이터 파일을 직접 패치하여 include 경로를 삽입.

## 현재 레시피 구조

```
recipes/
├── meta_version.txt          # 스택 공통 버전 (2026.04.13.00)
├── recipes.md                # 이 문서
├── folly/conanfile.py
├── fizz/conanfile.py
├── wangle/conanfile.py
├── mvfst/conanfile.py
└── proxygen/conanfile.py
```

### 레시피 의존성 그래프

```
proxygen
├── folly
├── fizz ──── folly
├── wangle ── folly, fizz
├── mvfst ─── folly, fizz
├── openssl, zlib, fmt, glog, gflags, libevent, c-ares
```

### 각 레시피 `package_info()` 요약

| 레시피 | `cmake_find_mode` | `cmake_target_name` | `builddirs` | 컴포넌트 |
|---|---|---|---|---|
| folly | `"both"` | `Folly::folly` | `lib/cmake/folly` | - |
| fizz | `"both"` | `fizz::fizz` | `lib/cmake/fizz` | - |
| wangle | `"both"` | `wangle::wangle` | `lib/cmake/wangle` | - |
| mvfst | `"both"` | `mvfst::mvfst` | `lib/cmake/mvfst` | - |
| proxygen | `"both"` | `proxygen::proxygen` | `lib/cmake/proxygen` | `proxygenhttpserver` |

## 빌드 방법

### 최초 빌드 (처음 클론 후)

```bash
cmake -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug
```

처음 실행 시:
1. Conan이 없으면 자동 설치
2. 5개 레시피가 Conan 캐시에 export
3. `conan install --build=missing`이 자동으로 folly → fizz → wangle → mvfst → proxygen 순서로 소스 빌드
4. 최초 빌드는 30~60분 소요 (이후에는 Conan 캐시에서 바로 사용)

### 버전 업그레이드

```bash
# 1. 버전 변경
echo "2026.05.01.00" > recipes/meta_version.txt

# 2. Conan 캐시 정리 (선택적)
./conan/bin/conan remove "folly/*" -c
./conan/bin/conan remove "fizz/*" -c
./conan/bin/conan remove "wangle/*" -c
./conan/bin/conan remove "mvfst/*" -c
./conan/bin/conan remove "proxygen/*" -c

# 3. cmake build 폴더 정리
rm -rf cmake-build-debug/conan

# 4. 재빌드
cmake -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug
```

### 디버깅 팁

Conan 캐시에서 실제 설치된 파일 확인:

```bash
# 패키지 폴더 확인
ls ~/.conan2/p/b/folly*/p/lib/cmake/folly/
ls ~/.conan2/p/b/proxy*/p/lib/

# 생성된 CMake 파일 확인
ls cmake-build-debug/conan/build/Debug/generators/ | grep proxygen

# Conan toolchain의 CMAKE_PREFIX_PATH 확인
grep CMAKE_PREFIX_PATH cmake-build-debug/conan/build/Debug/generators/conan_toolchain.cmake
```

## 요약

이 작업은 "Conan으로 오픈소스 하나를 추가한다" 수준이 아니라, Facebook 네트워크 스택 전체를 Conan 생태계에 맞게 번역하는 작업에 가깝다.

문제가 반복된 이유는 개별 에러 하나하나보다 다음 구조 충돌 때문이다.

- upstream native CMake 생태계 (세분화된 컴포넌트 타깃)
- Conan CMakeDeps 래퍼 생태계 (단순화된 타깃)
- 최종 프로젝트의 cmake-conan provider 소비 방식 (generators 폴더 우선 탐색)

이 세 가지를 조화시키는 핵심이 `package_info()` = `"both"` + `generate()` = `"none"` 오버라이드 패턴이다. consumer에서는 래퍼를, 내부 빌드에서는 native config를 사용하도록 분리함으로써 두 세계의 요구사항을 모두 만족시킨다.
