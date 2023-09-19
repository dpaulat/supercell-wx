from conans import ConanFile

class SupercellWxConan(ConanFile):
    settings   = ("os", "compiler", "build_type", "arch")
    requires   = ("boost/1.81.0",
                  "cpr/1.9.3",
                  "fontconfig/2.14.2",
                  "freetype/2.12.1",
                  "geographiclib/1.52",
                  "glew/2.2.0",
                  "glm/cci.20220420",
                  "gtest/1.13.0",
                  "libcurl/7.86.0",
                  "libxml2/2.10.3",
                  "openssl/3.1.0",
                  "spdlog/1.11.0",
                  "sqlite3/3.40.1",
                  "vulkan-loader/1.3.236.0",
                  "zlib/1.2.13")
    generators = ("cmake",
                  "cmake_find_package",
                  "cmake_paths")
    default_options = {"libiconv:shared"  : True,
                       "openssl:no_module": True,
                       "openssl:shared"   : True}

    def requirements(self):
        if self.settings.os == "Linux":
            self.requires("onetbb/2021.9.0")

    def imports(self):
        self.copy("*.dll", dst="bin", src="bin")
        self.copy("*.dylib", dst="bin", src="lib")
        self.copy("license*", dst="licenses", src=".", folder=True, ignore_case=True)
