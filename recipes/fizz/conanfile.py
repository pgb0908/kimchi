from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain
from conan.tools.files import get, copy, replace_in_file
import os
import re

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
        self.requires("fmt/12.1.0")
        self.requires("glog/0.7.1")
        self.requires("gflags/2.2.2")
        self.requires("zstd/1.5.5")
        self.requires("zlib/1.3.1")
        self.requires("libevent/2.1.12")

    def layout(self):
        cmake_layout(self)

    def source(self):
        get(self,
            url="https://github.com/facebookincubator/fizz/archive/refs/tags/v2026.04.13.00.tar.gz",
            strip_root=True)

    def generate(self):
        CMakeDeps(self).generate()
        tc = CMakeToolchain(self)
        for key, value in self._cmake_configure_variables().items():
            tc.cache_variables[key] = value
        tc.generate()
        self._patch_fmt_data()

    def _cmake_configure_variables(self):
        return {
            "BUILD_TESTS": "OFF",
            "BUILD_EXAMPLES": "OFF",
            "BUILD_SHARED_LIBS": "ON" if self.options.shared else "OFF",
            "sodium_USE_STATIC_LIBS": "OFF" if self.options.shared else "ON",
            "CMAKE_POLICY_VERSION_MINIMUM": "3.5",
        }

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
        self._patch_upstream_cmake()
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, "fizz"))
        cmake.build()

    def _patch_upstream_cmake(self):
        source_root = os.path.join(self.source_folder, "fizz")
        self._replace_regex_in_tree(source_root, r"\bFolly::[A-Za-z0-9_]+\b", "folly::folly")
        cmake_lists = os.path.join(source_root, "CMakeLists.txt")
        replace_in_file(
            self,
            cmake_lists,
            "find_package(Glog REQUIRED)",
            "find_package(glog CONFIG REQUIRED)",
            strict=False,
        )
        replace_in_file(
            self,
            cmake_lists,
            "find_package(Zstd REQUIRED)",
            "find_package(zstd CONFIG REQUIRED)",
            strict=False,
        )
        replace_in_file(
            self,
            cmake_lists,
            "find_package(Sodium REQUIRED)",
            "find_package(libsodium CONFIG REQUIRED)\nset(Sodium_FOUND TRUE)",
            strict=False,
        )
        replace_in_file(
            self,
            cmake_lists,
            "if(TARGET event)\n  message(STATUS \"Found libevent from package config\")\n  list(APPEND FIZZ_SHINY_DEPENDENCIES event)\nelse()\n  find_package(Libevent MODULE REQUIRED)\n  list(APPEND FIZZ_LINK_LIBRARIES ${LIBEVENT_LIB})\n  list(APPEND FIZZ_INCLUDE_DIRECTORIES ${LIBEVENT_INCLUDE_DIR})\nendif()",
            "if(TARGET libevent::core)\n  message(STATUS \"Found libevent from package config\")\n  list(APPEND FIZZ_LINK_LIBRARIES libevent::core)\nelse()\n  find_package(Libevent MODULE REQUIRED)\n  list(APPEND FIZZ_LINK_LIBRARIES ${LIBEVENT_LIB})\n  list(APPEND FIZZ_INCLUDE_DIRECTORIES ${LIBEVENT_INCLUDE_DIR})\nendif()",
            strict=False,
        )
        replace_in_file(
            self,
            cmake_lists,
            "sodium Threads::Threads ZLIB::ZLIB ${ZSTD_LIBRARY}",
            "libsodium::libsodium Threads::Threads ZLIB::ZLIB zstd::libzstd",
            strict=False,
        )
        replace_in_file(
            self,
            cmake_lists,
            "${GLOG_LIBRARIES} ${GFLAGS_LIBRARIES} ${FIZZ_LINK_LIBRARIES} ${CMAKE_DL_LIBS} ${LIBRT_LIBRARIES}",
            "glog::glog ${GFLAGS_LIBRARIES} ${FIZZ_LINK_LIBRARIES} ${CMAKE_DL_LIBS} ${LIBRT_LIBRARIES}",
            strict=False,
        )

    def _replace_regex_in_file(self, path, pattern, repl):
        with open(path, "r", encoding="utf-8") as f:
            content = f.read()
        updated = re.sub(pattern, repl, content)
        if updated != content:
            with open(path, "w", encoding="utf-8") as f:
                f.write(updated)

    def _replace_regex_in_tree(self, root, pattern, repl):
        for current_root, _, files in os.walk(root):
            for name in files:
                if name == "CMakeLists.txt" or name.endswith(".cmake"):
                    self._replace_regex_in_file(os.path.join(current_root, name), pattern, repl)

    def package(self):
        cmake = CMake(self)
        cmake.install()
        # cmake install이 누락하는 헤더를 명시적으로 복사 (backend/openssl 등)
        copy(self, "*.h",
             src=os.path.join(self.source_folder, "fizz"),
             dst=os.path.join(self.package_folder, "include", "fizz"),
             keep_path=True)

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "fizz")
        self.cpp_info.set_property("cmake_target_name", "fizz::fizz")
        self.cpp_info.builddirs = ["lib/cmake/fizz"]
        self.cpp_info.libs = ["fizz"]
