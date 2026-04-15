from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMakeToolchain, CMakeDeps
import os

class ConanApplication(ConanFile):
    package_type = "application"
    settings = "os", "compiler", "build_type", "arch"

    def _meta_version(self):
        p = os.path.join(os.path.dirname(__file__), "recipes", "meta_version.txt")
        return open(p).read().strip()

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.user_presets_path = False
        tc.generate()

    def requirements(self):
        self.requires("gtest/1.14.0")
        self.requires("glog/0.7.1")
        self.requires("gflags/2.2.2")
        self.requires(f"proxygen/{self._meta_version()}")

