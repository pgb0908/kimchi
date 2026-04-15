from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain
from conan.tools.files import get, replace_in_file
import os

class WangleConan(ConanFile):
    name = "wangle"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def set_version(self):
        p = os.path.join(os.path.dirname(__file__), "..", "meta_version.txt")
        self.version = open(p).read().strip()

    def requirements(self):
        self.requires(f"folly/{self.version}")
        self.requires(f"fizz/{self.version}")
        self.requires("openssl/3.3.2")

    def layout(self):
        cmake_layout(self)

    def source(self):
        get(self,
            url=f"https://github.com/facebook/wangle/archive/refs/tags/v{self.version}.tar.gz",
            strip_root=True)

    def generate(self):
        deps = CMakeDeps(self)
        # 내부 빌드에서는 native config 사용 (컴포넌트 타깃 필요)
        deps.set_property("folly", "cmake_find_mode", "none")
        deps.set_property("fizz", "cmake_find_mode", "none")
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
            "CMAKE_POLICY_VERSION_MINIMUM": "3.5",
        }

    def build(self):
        self._patch_upstream_cmake()
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, "wangle"))
        cmake.build()

    def _patch_upstream_cmake(self):
        source_root = os.path.join(self.source_folder, "wangle")
        cmake_lists = os.path.join(source_root, "CMakeLists.txt")
        replace_in_file(
            self,
            cmake_lists,
            "find_package(Fizz CONFIG REQUIRED)",
            "find_package(fizz CONFIG REQUIRED)",
            strict=False,
        )
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
            "find_package(Gflags REQUIRED)",
            "find_package(gflags CONFIG REQUIRED)",
            strict=False,
        )

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("cmake_file_name", "wangle")
        self.cpp_info.set_property("cmake_target_name", "wangle::wangle")
        self.cpp_info.builddirs = ["lib/cmake/wangle"]
        self.cpp_info.libs = ["wangle"]
