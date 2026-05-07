from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMakeToolchain, CMakeDeps
import os
from pathlib import Path
import re

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
        self._patch_generated_include_dirs()
        tc = CMakeToolchain(self)
        tc.user_presets_path = False
        tc.generate()

    def _patch_generated_include_dirs(self):
        generators_dir = Path(self.generators_folder)
        for data_file in generators_dir.glob("*-data.cmake"):
            contents = data_file.read_text()

            package_folder_vars = {
                config: package_var
                for package_var, config in re.findall(
                    r"set\(([A-Za-z0-9_]+)_PACKAGE_FOLDER_(DEBUG|RELEASE|RELWITHDEBINFO|MINSIZEREL)\s+\"[^\"]+\"\)",
                    contents,
                )
            }
            if not package_folder_vars:
                continue

            updated = contents
            for config, package_var in package_folder_vars.items():
                updated = re.sub(
                    rf"set\(([A-Za-z0-9_]+_INCLUDE_DIRS_{config})\s*\)",
                    rf'set(\1 "${{{package_var}_PACKAGE_FOLDER_{config}}}/include")',
                    updated,
                )

            if updated != contents:
                data_file.write_text(updated)

    def requirements(self):
        self.requires("glog/0.7.1")
        self.requires("gflags/2.2.2")
        self.requires(f"proxygen/{self._meta_version()}")
        self.requires("zlib/1.3.1", override=True)
        self.requires("jwt-cpp/0.7.0")
