from conan import ConanFile
from conan.tools.cmake import CMake
from conan.tools.files import copy

class SupercellWxConan(ConanFile):
    settings   = ("os", "compiler", "build_type", "arch")
    requires   = ("boost/1.83.0",
                  "cpr/1.10.5",
                  "fontconfig/2.14.2",
                  "freetype/2.13.2",
                  "geographiclib/2.3",
                  "geos/3.12.0",
                  "glew/2.2.0",
                  "glm/cci.20230113",
                  "gtest/1.14.0",
                  "libcurl/8.4.0",
                  "libxml2/2.12.2",
                  "openssl/3.2.0",
                  "re2/20231101",
                  "spdlog/1.12.0",
                  "sqlite3/3.44.2",
                  "vulkan-loader/1.3.243.0",
                  "zlib/1.3")
    generators = ("CMakeDeps")
    default_options = {"geos/*:shared"      : True,
                       "libiconv/*:shared"  : True,
                       "openssl/*:no_module": True,
                       "openssl/*:shared"   : True}

    def requirements(self):
        if self.settings.os == "Linux":
            self.requires("onetbb/2021.10.0")

    def generate(self):
        for dep in self.dependencies.values():
            if dep.cpp_info.libdirs:
                copy(self, "*.dll", dep.cpp_info.libdirs[0], self.build_folder)
                copy(self, "*.dylib", dep.cpp_info.libdirs[0], self.build_folder)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
