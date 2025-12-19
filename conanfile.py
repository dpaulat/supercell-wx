from conan import ConanFile
from conan.tools.cmake import CMake
from conan.tools.files import copy
import os

class SupercellWxConan(ConanFile):
    settings   = ("os", "compiler", "build_type", "arch")
    requires   = ("boost/1.89.0",
                  "cpr/1.14.1",
                  "fontconfig/2.15.0",
                  "geographiclib/2.6",
                  "geos/3.13.0",
                  "glm/1.0.1",
                  "gtest/1.17.0",
                  "libcurl/8.17.0",
                  "libpng/1.6.53",
                  "libxml2/2.15.0",
                  "libzip/1.11.4",
                  "openssl/3.5.0",
                  "range-v3/cci.20240905",
                  "re2/20251105",
                  "spdlog/1.16.0",
                  "sqlite3/3.51.0",
                  "vulkan-loader/1.4.313.0",
                  "zlib/1.3.1")
    generators = ("CMakeDeps")
    default_options = {"geos/*:shared"     : True,
                       "libiconv/*:shared" : True}

    def configure(self):
        if self.settings.os == "Windows":
            self.options["libcurl"].with_ssl = "schannel"
        elif self.settings.os == "Linux":
            self.options["openssl"].shared    = True
            self.options["libcurl"].ca_bundle = "none"
            self.options["libcurl"].ca_path   = "none"
        elif self.settings.os == "Macos":
            self.options["openssl"].shared    = True
            self.options["libcurl"].ca_bundle = "none"
            self.options["libcurl"].ca_path   = "none"

    def requirements(self):
        if self.settings.os == "Linux":
            self.requires("opengl/system")
            self.requires("glu/system")
            self.requires("onetbb/2022.3.0")

        # Force dependency graph (fontconfig) to use a newer version of freetype
        self.requires("freetype/2.14.1", force=True)

    def generate(self):
        build_folder = os.path.join(self.build_folder,
                                    "..",
                                    str(self.settings.build_type),
                                    self.cpp_info.bindirs[0])

        for dep in self.dependencies.values():
            if dep.cpp_info.bindirs:
                copy(self, "*.dll", dep.cpp_info.bindirs[0], build_folder)
            if dep.cpp_info.libdirs:
                copy(self, "*.dylib", dep.cpp_info.libdirs[0], build_folder)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
