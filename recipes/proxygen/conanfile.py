from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain
from conan.tools.files import get
import os

class ProxygenConan(ConanFile):
    name = "proxygen"
    version = "2024.08.12.00"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def requirements(self):
        self.requires("folly/2024.08.12.00")
        self.requires("fizz/2024.08.12.00")
        self.requires("wangle/2024.08.12.00")
        self.requires("openssl/3.3.2")
        self.requires("zlib/1.3.1")
        self.requires("fmt/10.2.1")

    def layout(self):
        cmake_layout(self)

    def source(self):
        get(self,
            url="https://github.com/facebook/proxygen/archive/refs/tags/v2024.08.12.00.tar.gz",
            strip_root=True)

    def generate(self):
        CMakeDeps(self).generate()
        CMakeToolchain(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(
            build_script_folder=os.path.join(self.source_folder, "proxygen"),
            variables={
                "BUILD_TESTS": "OFF",
                "BUILD_SAMPLES": "OFF",
            }
        )
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["proxygenlib", "proxygenhttpserver"]
