from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain
from conan.tools.files import get, copy
import os

class FizzConan(ConanFile):
    name = "fizz"
    version = "2024.08.12.00"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def requirements(self):
        self.requires("folly/2024.08.12.00")
        self.requires("openssl/3.3.2")
        self.requires("libsodium/1.0.19")

    def layout(self):
        cmake_layout(self)

    def source(self):
        get(self,
            url="https://github.com/facebookincubator/fizz/archive/refs/tags/v2024.08.12.00.tar.gz",
            strip_root=True)

    def generate(self):
        CMakeDeps(self).generate()
        CMakeToolchain(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(
            build_script_folder=os.path.join(self.source_folder, "fizz"),
            variables={
                "BUILD_TESTS": "OFF",
                "BUILD_EXAMPLES": "OFF",
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
