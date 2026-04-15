from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain
from conan.tools.files import get, replace_in_file
import os
import re

class ProxygenConan(ConanFile):
    name = "proxygen"
    version = "2026.04.13.00"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def requirements(self):
        self.requires("folly/2026.04.13.00")
        self.requires("fizz/2026.04.13.00")
        self.requires("wangle/2026.04.13.00")
        self.requires("openssl/3.3.2")
        self.requires("zlib/1.3.1")
        self.requires("fmt/12.1.0")
        self.requires("glog/0.7.1")
        self.requires("gflags/2.2.2")
        self.requires("libevent/2.1.12")
        self.requires("c-ares/1.34.6")

    def layout(self):
        cmake_layout(self)

    def source(self):
        get(self,
            url="https://github.com/facebook/proxygen/archive/refs/tags/v2026.04.13.00.tar.gz",
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
            "BUILD_SAMPLES": "OFF",
            "BUILD_QUIC": "OFF",
            "BUILD_SHARED_LIBS": "ON" if self.options.shared else "OFF",
            "CMAKE_POLICY_VERSION_MINIMUM": "3.5",
        }

    def build(self):
        self._patch_upstream_cmake()
        cmake = CMake(self)
        cmake.configure(build_script_folder=self.source_folder)
        cmake.build()

    def _patch_upstream_cmake(self):
        source_root = self.source_folder
        self._replace_regex_in_tree(source_root, r"\bFolly::[A-Za-z0-9_]+\b", "folly::folly")
        cmake_lists = os.path.join(source_root, "CMakeLists.txt")
        self._replace_regex_in_file(
            cmake_lists,
            r'if \(NOT DEFINED mvfst_SOURCE_DIR\)\s*find_package\(mvfst REQUIRED\)\s*endif\(\)\s*',
            "",
        )
        replace_in_file(
            self,
            cmake_lists,
            "find_package(Wangle CONFIG REQUIRED)",
            "find_package(wangle CONFIG REQUIRED)",
            strict=False,
        )
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
        replace_in_file(
            self,
            cmake_lists,
            "find_package(mvfst CONFIG REQUIRED)",
            "",
            strict=False,
        )
        replace_in_file(
            self,
            cmake_lists,
            "find_package(mvfst REQUIRED)",
            "",
            strict=False,
        )
        self._patch_out_mvfst_consumers(source_root)

    def _replace_regex_in_file(self, path, pattern, repl):
        with open(path, "r", encoding="utf-8") as f:
            content = f.read()
        updated = re.sub(pattern, repl, content, flags=re.DOTALL)
        if updated != content:
            with open(path, "w", encoding="utf-8") as f:
                f.write(updated)

    def _replace_regex_in_tree(self, root, pattern, repl):
        for current_root, _, files in os.walk(root):
            for name in files:
                if name == "CMakeLists.txt" or name.endswith(".cmake"):
                    self._replace_regex_in_file(os.path.join(current_root, name), pattern, repl)

    def _patch_out_mvfst_consumers(self, source_root):
        lib_cmake = os.path.join(source_root, "proxygen", "lib", "CMakeLists.txt")
        http_cmake = os.path.join(source_root, "proxygen", "lib", "http", "CMakeLists.txt")

        self._replace_regex_in_file(
            lib_cmake,
            r"set\(\s*HTTP3_SOURCES.*?\)\s*set\(\s*HTTP3_DEPEND_LIBS.*?\)\s*",
            "",
        )
        self._replace_regex_in_file(
            lib_cmake,
            r"target_link_libraries\(proxygen\s+PUBLIC\s+.*?mvfst::.*?\)\s*",
            "",
        )
        replace_in_file(self, lib_cmake, "add_subdirectory(transport)\n", "", strict=False)

        self._replace_regex_in_file(
            http_cmake,
            r"proxygen_add_library\(proxygen_http_hq_connector.*?\n\)\n\n",
            "",
        )
        self._replace_regex_in_file(
            http_cmake,
            r"proxygen_add_library\(proxygen_http_h3_errors.*?\n\)\n\n",
            "",
        )
        self._replace_regex_in_file(
            http_cmake,
            r"proxygen_add_library\(proxygen_http_synchronized_quic_lrucache.*?\n\)\n\n",
            "",
        )
        replace_in_file(self, http_cmake, "add_subdirectory(webtransport)\n", "", strict=False)

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "proxygen")
        self.cpp_info.components["proxygenlib"].set_property("cmake_target_name", "proxygen::proxygenlib")
        self.cpp_info.components["proxygenlib"].libs = ["proxygenlib"]
        self.cpp_info.components["proxygenhttpserver"].set_property("cmake_target_name", "proxygen::proxygenhttpserver")
        self.cpp_info.components["proxygenhttpserver"].libs = ["proxygenhttpserver"]
        self.cpp_info.components["proxygenhttpserver"].requires = ["proxygenlib"]
