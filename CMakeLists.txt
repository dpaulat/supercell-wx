cmake_minimum_required(VERSION 3.20)
set(PROJECT_NAME supercell-wx)
project(${PROJECT_NAME} C CXX)

set(CMAKE_POLICY_DEFAULT_CMP0054 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0079 NEW)

enable_testing()

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

include(${PROJECT_SOURCE_DIR}/external/cmake-conan/conan.cmake)

conan_cmake_configure(REQUIRES boost/1.81.0
                               cpr/1.9.3
                               freetype/2.12.1
                               geographiclib/1.52
                               glew/2.2.0
                               glm/cci.20220420
                               gtest/cci.20210126
                               libcurl/7.85.0
                               libxml2/2.10.3
                               openssl/1.1.1s
                               spdlog/1.11.0
                               sqlite3/3.40.1
                               vulkan-loader/1.3.236.0
                               zlib/1.2.13
                      GENERATORS cmake
                                 cmake_find_package
                                 cmake_paths
                      IMPORTS "bin, *.dll -> ./bin"
                      IMPORTS "lib, *.dylib -> ./bin"
                      OPTIONS openssl:shared=True)

conan_cmake_autodetect(settings)

conan_cmake_install(PATH_OR_REFERENCE .
                    BUILD missing
                    REMOTE conancenter
                    SETTINGS ${settings})

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
include(${CMAKE_BINARY_DIR}/conan_paths.cmake)
conan_basic_setup(TARGETS)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_ALL_NO_LIB")

set(SCWX_DIR ${PROJECT_SOURCE_DIR})

option(BUILD_DOCS "Build documentation" OFF)

add_subdirectory(external)
add_subdirectory(wxdata)
add_subdirectory(scwx-qt)
add_subdirectory(test)

if(BUILD_DOCS)
    add_subdirectory(docs)
endif()
