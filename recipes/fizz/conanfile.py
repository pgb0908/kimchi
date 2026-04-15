from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain
from conan.tools.files import get, copy, replace_in_file
import os

class FizzConan(ConanFile):
    name = "fizz"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def set_version(self):
        p = os.path.join(os.path.dirname(__file__), "..", "meta_version.txt")
        self.version = open(p).read().strip()

    def requirements(self):
        self.requires(f"folly/{self.version}")
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
            url=f"https://github.com/facebookincubator/fizz/archive/refs/tags/v{self.version}.tar.gz",
            strip_root=True)

    def generate(self):
        deps = CMakeDeps(self)
        # 내부 빌드에서는 folly native config 사용 (Folly::folly_range 등 컴포넌트 타깃 필요)
        deps.set_property("folly", "cmake_find_mode", "none")
        deps.generate()
        tc = CMakeToolchain(self)
        for key, value in self._cmake_configure_variables().items():
            tc.cache_variables[key] = value
        tc.generate()

    def _cmake_configure_variables(self):
        return {
            "BUILD_TESTS": "OFF",
            "BUILD_EXAMPLES": "OFF",
            "BUILD_SHARED_LIBS": "ON" if self.options.shared else "OFF",
            "sodium_USE_STATIC_LIBS": "OFF" if self.options.shared else "ON",
            "CMAKE_POLICY_VERSION_MINIMUM": "3.5",
        }

    def build(self):
        self._patch_upstream_cmake()
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, "fizz"))
        cmake.build()

    def _patch_upstream_cmake(self):
        source_root = os.path.join(self.source_folder, "fizz")
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

    def package(self):
        cmake = CMake(self)
        cmake.install()
        # cmake install이 누락하는 헤더를 명시적으로 복사 (backend/openssl 등)
        copy(self, "*.h",
             src=os.path.join(self.source_folder, "fizz"),
             dst=os.path.join(self.package_folder, "include", "fizz"),
             keep_path=True)

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("cmake_file_name", "fizz")
        self.cpp_info.set_property("cmake_target_name", "fizz::fizz")
        self.cpp_info.builddirs = ["lib/cmake/fizz"]
        self.cpp_info.libs = ["fizz"]
