from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain
from conan.tools.files import get, replace_in_file
import os
import re

class WangleConan(ConanFile):
    name = "wangle"
    version = "2026.04.13.00"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def requirements(self):
        self.requires("folly/2026.04.13.00")
        self.requires("fizz/2026.04.13.00")
        self.requires("openssl/3.3.2")

    def layout(self):
        cmake_layout(self)

    def source(self):
        get(self,
            url="https://github.com/facebook/wangle/archive/refs/tags/v2026.04.13.00.tar.gz",
            strip_root=True)

    def generate(self):
        CMakeDeps(self).generate()
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
        self._replace_regex_in_tree(source_root, r"\bFolly::[A-Za-z0-9_]+\b", "folly::folly")
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

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "wangle")
        self.cpp_info.set_property("cmake_target_name", "wangle::wangle")
        self.cpp_info.builddirs = ["lib/cmake/wangle"]
        self.cpp_info.libs = ["wangle"]
