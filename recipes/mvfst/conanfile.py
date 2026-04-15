from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain
from conan.tools.files import get, collect_libs, replace_in_file
import os


class MvfstConan(ConanFile):
    name = "mvfst"
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
        self.requires("libevent/2.1.12")
        self.requires("glog/0.7.1")
        self.requires("gflags/2.2.2")
        self.requires("zlib/1.3.1")

    def layout(self):
        cmake_layout(self)

    def source(self):
        get(
            self,
            url=f"https://github.com/facebook/mvfst/archive/refs/tags/v{self.version}.tar.gz",
            strip_root=True,
        )

    def generate(self):
        deps = CMakeDeps(self)
        # 내부 빌드에서는 native config 사용 (컴포넌트 타깃 필요)
        deps.set_property("folly", "cmake_find_mode", "none")
        deps.set_property("fizz", "cmake_find_mode", "none")
        deps.generate()
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_TESTS"] = "OFF"
        tc.cache_variables["BUILD_SAMPLES"] = "OFF"
        tc.cache_variables["BUILD_SHARED_LIBS"] = "ON" if self.options.shared else "OFF"
        tc.cache_variables["CMAKE_POLICY_VERSION_MINIMUM"] = "3.5"
        tc.generate()
        self._patch_glog_data()

    def build(self):
        self._patch_upstream_cmake()
        cmake = CMake(self)
        cmake.configure(build_script_folder=self.source_folder)
        cmake.build()

    def _patch_upstream_cmake(self):
        cmake_lists = os.path.join(self.source_folder, "CMakeLists.txt")
        replace_in_file(
            self,
            cmake_lists,
            "find_package(OpenSSL REQUIRED)",
            "find_package(OpenSSL REQUIRED)\nfind_package(Libevent REQUIRED)",
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
            "find_package(Fizz CONFIG REQUIRED)",
            "find_package(fizz REQUIRED)",
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
        self.cpp_info.set_property("cmake_file_name", "mvfst")
        self.cpp_info.set_property("cmake_target_name", "mvfst::mvfst")
        self.cpp_info.builddirs = ["lib/cmake/mvfst"]
        self.cpp_info.libs = collect_libs(self)
