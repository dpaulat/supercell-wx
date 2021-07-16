cmake_minimum_required(VERSION 3.11)
set(PROJECT_NAME supercell-wx)
project(${PROJECT_NAME} CXX)

set(CMAKE_POLICY_DEFAULT_CMP0054 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0079 NEW)

enable_testing()

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

include(${PROJECT_SOURCE_DIR}/external/cmake-conan/conan.cmake)

conan_cmake_configure(REQUIRES boost/1.76.0
                               glm/0.9.9.8
                               gtest/cci.20210126
                               openssl/1.1.1k
                               vulkan-loader/1.2.172
                      GENERATORS cmake
                                 cmake_find_package
                                 cmake_paths
                      BUILD_REQUIRES cmake/3.19.4
                      IMPORTS "bin, *.dll -> ./bin"
                      IMPORTS "lib, *.dylib -> ./bin"
                      OPTIONS openssl:shared=True)

conan_cmake_autodetect(settings)

conan_cmake_install(PATH_OR_REFERENCE .
                    BUILD missing
                    REMOTE conan-center
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
