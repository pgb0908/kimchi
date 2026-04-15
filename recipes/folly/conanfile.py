from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain
from conan.tools.files import get

class FollyConan(ConanFile):
    name = "folly"
    version = "2026.04.13.00"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def requirements(self):
        self.requires("openssl/3.3.2")
        self.requires("zlib/1.3.1")
        self.requires("fmt/10.2.1")
        self.requires("gflags/2.2.2")
        self.requires("glog/0.7.1")
        self.requires("double-conversion/3.3.0")
        self.requires("libevent/2.1.12")
        self.requires("boost/1.85.0")
        self.requires("libsodium/1.0.19")
        self.requires("lz4/1.9.4")
        self.requires("zstd/1.5.5")
        self.requires("bzip2/1.0.8")
        self.requires("xz_utils/5.4.5")
        self.requires("snappy/1.1.10")
        self.requires("fast_float/8.1.0")

    def layout(self):
        cmake_layout(self)

    def source(self):
        get(self,
            url="https://github.com/facebook/folly/archive/refs/tags/v2026.04.13.00.tar.gz",
            strip_root=True)

    def generate(self):
        CMakeDeps(self).generate()
        CMakeToolchain(self).generate()

    def build(self):
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

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["folly"]
        self.cpp_info.system_libs = ["pthread", "dl"]
