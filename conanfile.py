from conans import ConanFile

class SupercellWxConan(ConanFile):
    settings   = ("os", "compiler", "build_type", "arch")
    requires   = ("boost/1.83.0",
                  "cpr/1.10.5",
                  "fontconfig/2.14.2",
                  "geographiclib/2.3",
                  "glew/2.2.0",
                  "glm/cci.20230113",
                  "gtest/1.14.0",
                  "libcurl/8.4.0",
                  "libxml2/2.11.5",
                  "openssl/3.1.4",
                  "spdlog/1.12.0",
                  "sqlite3/3.44.2",
                  "vulkan-loader/1.3.243.0",
                  "zlib/1.3")
    generators = ("cmake",
                  "cmake_find_package",
                  "cmake_paths")
    default_options = {"libiconv:shared"  : True,
                       "openssl:no_module": True,
                       "openssl:shared"   : True}

    def requirements(self):
        if self.settings.os == "Linux":
            self.requires("onetbb/2021.10.0")

    def imports(self):
        self.copy("*.dll", dst="bin", src="bin")
        self.copy("*.dylib", dst="bin", src="lib")
        self.copy("license*", dst="licenses", src=".", folder=True, ignore_case=True)
