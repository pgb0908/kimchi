from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain
from conan.tools.files import get, replace_in_file
import os

class FollyConan(ConanFile):
    name = "folly"
    settings = "os", "compiler", "build_type", "arch"

    def set_version(self):
        p = os.path.join(os.path.dirname(__file__), "..", "meta_version.txt")
        self.version = open(p).read().strip()
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def requirements(self):
        self.requires("openssl/3.3.2", transitive_headers=True, transitive_libs=True)
        self.requires("zlib/1.3.1", transitive_headers=True, transitive_libs=True)
        self.requires("fmt/12.1.0", transitive_headers=True, transitive_libs=True)
        self.requires("gflags/2.2.2", transitive_headers=True, transitive_libs=True)
        self.requires("glog/0.7.1", transitive_headers=True, transitive_libs=True)
        self.requires("double-conversion/3.3.0", transitive_headers=True, transitive_libs=True)
        self.requires("libevent/2.1.12", transitive_headers=True, transitive_libs=True)
        self.requires("boost/1.85.0", transitive_headers=True, transitive_libs=True)
        self.requires("libsodium/1.0.19", transitive_headers=True, transitive_libs=True)
        self.requires("lz4/1.9.4", transitive_headers=True, transitive_libs=True)
        self.requires("zstd/1.5.5", transitive_headers=True, transitive_libs=True)
        self.requires("bzip2/1.0.8", transitive_headers=True, transitive_libs=True)
        self.requires("xz_utils/5.4.5", transitive_headers=True, transitive_libs=True)
        self.requires("snappy/1.1.10", transitive_headers=True, transitive_libs=True)
        self.requires("fast_float/8.1.0", transitive_headers=True, transitive_libs=True)

    def layout(self):
        cmake_layout(self)

    def source(self):
        get(self,
            url=f"https://github.com/facebook/folly/archive/refs/tags/v{self.version}.tar.gz",
            strip_root=True)

    def generate(self):
        CMakeDeps(self).generate()
        CMakeToolchain(self).generate()

    def build(self):
        self._patch_upstream_cmake()
        cmake = CMake(self)
        cmake.configure(
            variables={
                "BUILD_TESTS": "OFF",
                "BUILD_BENCHMARKS": "OFF",
                "FOLLY_USE_JEMALLOC": "OFF",
                "CMAKE_POLICY_VERSION_MINIMUM": "3.5",
            }
        )
        cmake.build()

    def _patch_upstream_cmake(self):
        folly_deps = os.path.join(self.source_folder, "CMake", "folly-deps.cmake")
        replace_in_file(
            self,
            folly_deps,
            "  list(APPEND FOLLY_CXX_FLAGS -DGLOG_USE_GLOG_EXPORT)\n",
            "",
            strict=False,
        )

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        # consumer에서는 CMakeDeps 래퍼를 통해 find_package(folly) 동작.
        # 내부 빌드(conan create)에서는 builddirs의 native config가 사용되어
        # Folly::folly, Folly::folly_range 등 세분화된 타깃을 제공.
        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("cmake_file_name", "folly")
        self.cpp_info.set_property("cmake_target_name", "Folly::folly")
        self.cpp_info.builddirs = ["lib/cmake/folly"]
        self.cpp_info.libs = ["folly"]
        self.cpp_info.system_libs = ["pthread", "dl"]
