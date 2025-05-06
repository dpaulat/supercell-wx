from conan import ConanFile
from conan.tools.cmake import CMake
from conan.tools.files import copy
import os

class SupercellWxConan(ConanFile):
    settings   = ("os", "compiler", "build_type", "arch")
    requires   = ("boost/1.87.0",
                  "cpr/1.11.2",
                  "fontconfig/2.15.0",
                  "freetype/2.13.2",
                  "geographiclib/2.4",
                  "geos/3.13.0",
                  "glew/2.2.0",
                  "glm/1.0.1",
                  "gtest/1.16.0",
                  "libcurl/8.12.1",
                  "libpng/1.6.47",
                  "libxml2/2.13.6",
                  "openssl/3.4.1",
                  "range-v3/cci.20240905",
                  "re2/20240702",
                  "spdlog/1.15.1",
                  "sqlite3/3.49.1",
                  "vulkan-loader/1.3.290.0",
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

    def requirements(self):
        if self.settings.os == "Linux":
            self.requires("onetbb/2022.0.0")

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
