from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain
from conan.tools.files import get, replace_in_file
import os

class ProxygenConan(ConanFile):
    name = "proxygen"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def set_version(self):
        p = os.path.join(os.path.dirname(__file__), "..", "meta_version.txt")
        self.version = open(p).read().strip()

    def requirements(self):
        self.requires(f"folly/{self.version}")
        self.requires(f"fizz/{self.version}")
        self.requires(f"wangle/{self.version}")
        self.requires(f"mvfst/{self.version}")
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
            url=f"https://github.com/facebook/proxygen/archive/refs/tags/v{self.version}.tar.gz",
            strip_root=True)

    def generate(self):
        deps = CMakeDeps(self)
        # 내부 빌드에서는 native config 사용 (컴포넌트 타깃 필요)
        for pkg in ["folly", "fizz", "wangle", "mvfst"]:
            deps.set_property(pkg, "cmake_find_mode", "none")
        deps.generate()
        tc = CMakeToolchain(self)
        for key, value in self._cmake_configure_variables().items():
            tc.cache_variables[key] = value
        tc.generate()
        self._patch_glog_data()

    def _cmake_configure_variables(self):
        return {
            "BUILD_TESTS": "OFF",
            "BUILD_SAMPLES": "OFF",
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
        cmake_lists = os.path.join(source_root, "CMakeLists.txt")
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
            "if (NOT DEFINED fizz_SOURCE_DIR)\n  find_package(fizz REQUIRED)\nendif()",
            "find_package(Libevent REQUIRED)\nif (NOT DEFINED fizz_SOURCE_DIR)\n  find_package(fizz REQUIRED)\nendif()",
            strict=False,
        )

    def _patch_glog_data(self):
        build_type = str(self.settings.build_type).upper()
        arch = str(self.settings.arch)
        data_file = os.path.join(
            self.generators_folder,
            f"glog-{str(self.settings.build_type).lower()}-{arch}-data.cmake",
        )
        if not os.path.exists(data_file):
            return
        with open(data_file, "r", encoding="utf-8") as f:
            content = f.read()
        content = content.replace('set(glog_DEFINITIONS_%s "-DGFLAGS_DLL_DECLARE_FLAG="\n\t\t\t"-DGFLAGS_DLL_DEFINE_FLAG="\n\t\t\t"-DGLOG_USE_GLOG_EXPORT=")\n' % build_type,
                                  'set(glog_DEFINITIONS_%s "-DGLOG_USE_GLOG_EXPORT=")\n' % build_type)
        content = content.replace('set(glog_COMPILE_DEFINITIONS_%s "GFLAGS_DLL_DECLARE_FLAG="\n\t\t\t"GFLAGS_DLL_DEFINE_FLAG="\n\t\t\t"GLOG_USE_GLOG_EXPORT=")\n' % build_type,
                                  'set(glog_COMPILE_DEFINITIONS_%s "GLOG_USE_GLOG_EXPORT=")\n' % build_type)
        with open(data_file, "w", encoding="utf-8") as f:
            f.write(content)

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("cmake_file_name", "proxygen")
        self.cpp_info.set_property("cmake_target_name", "proxygen::proxygen")
        self.cpp_info.builddirs = ["lib/cmake/proxygen"]
        self.cpp_info.libs = ["proxygen"]
        self.cpp_info.components["proxygenhttpserver"].set_property(
            "cmake_target_name", "proxygen::proxygenhttpserver"
        )
        self.cpp_info.components["proxygenhttpserver"].libs = ["proxygen_httpserver"]
        self.cpp_info.components["proxygenhttpserver"].requires = ["proxygen"]
        self.cpp_info.components["proxygen"].set_property(
            "cmake_target_name", "proxygen::proxygen"
        )
        self.cpp_info.components["proxygen"].libs = ["proxygen"]
