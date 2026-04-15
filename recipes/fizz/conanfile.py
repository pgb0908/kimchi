from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain
from conan.tools.files import get, copy
import os

class FizzConan(ConanFile):
    name = "fizz"
    version = "2026.04.13.00"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def requirements(self):
        self.requires("folly/2026.04.13.00")
        self.requires("openssl/3.3.2")
        self.requires("libsodium/1.0.19")

    def layout(self):
        cmake_layout(self)

    def source(self):
        get(self,
            url="https://github.com/facebookincubator/fizz/archive/refs/tags/v2026.04.13.00.tar.gz",
            strip_root=True)

    def generate(self):
        CMakeDeps(self).generate()
        CMakeToolchain(self).generate()
        self._patch_fmt_data()

    def _patch_fmt_data(self):
        # Conan CMakeDeps 버그: fmt recipe가 components["_fmt"]를 사용하면서
        # includedirs를 명시하지 않아 INTERFACE_INCLUDE_DIRECTORIES가 빈 값으로 생성됨.
        # conan_toolchain.cmake가 CMAKE_FIND_PACKAGE_PREFER_CONFIG ON을 설정하므로
        # Findfmt.cmake 모듈 방식은 동작하지 않음.
        # 해결: CMakeDeps가 생성한 데이터 파일을 직접 패치.
        import re
        build_type = str(self.settings.build_type).lower()
        arch = str(self.settings.arch)
        data_file = os.path.join(
            self.generators_folder,
            f"fmt-{build_type}-{arch}-data.cmake"
        )
        if not os.path.exists(data_file):
            return
        with open(data_file, "r") as f:
            content = f.read()
        # 이미 패치됐으면 건너뜀
        if "INCLUDE_DIRS" in content and "/include" in content.split("fmt_INCLUDE_DIRS")[1][:100]:
            return
        # 데이터 파일에서 패키지 경로 추출
        bt = build_type.upper()
        match = re.search(rf'set\(fmt_PACKAGE_FOLDER_{bt}\s+"([^"]+)"\)', content)
        if not match:
            return
        include_dir = match.group(1) + "/include"
        content = content.replace(
            f"set(fmt_fmt_fmt_INCLUDE_DIRS_{bt} )\n",
            f'set(fmt_fmt_fmt_INCLUDE_DIRS_{bt} "{include_dir}")\n'
        )
        content = content.replace(
            f"set(fmt_INCLUDE_DIRS_{bt} )\n",
            f'set(fmt_INCLUDE_DIRS_{bt} "{include_dir}")\n'
        )
        with open(data_file, "w") as f:
            f.write(content)

    def build(self):
        cmake = CMake(self)
        cmake.configure(
            build_script_folder=os.path.join(self.source_folder, "fizz"),
            variables={
                "BUILD_TESTS": "OFF",
                "BUILD_EXAMPLES": "OFF",
                "CMAKE_POLICY_VERSION_MINIMUM": "3.5",
            }
        )
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        # cmake install이 누락하는 헤더를 명시적으로 복사 (backend/openssl 등)
        copy(self, "*.h",
             src=os.path.join(self.source_folder, "fizz"),
             dst=os.path.join(self.package_folder, "include", "fizz"),
             keep_path=True)

    def package_info(self):
        self.cpp_info.libs = ["fizz"]
