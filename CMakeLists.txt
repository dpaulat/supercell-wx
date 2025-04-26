cmake_minimum_required(VERSION 3.24)
set(PROJECT_NAME supercell-wx)
project(${PROJECT_NAME}
        VERSION      1.0.0
        DESCRIPTION  "TorDar is a free and open source fork of Supercell WX. TorDar is aimed to both revamp Supercell WX and add new features that were never there before."
        HOMEPAGE_URL "https://github.com/TornadoRTX/TorDar"
        LANGUAGES    C CXX)

set(CMAKE_POLICY_DEFAULT_CMP0054 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0079 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0148 OLD) # aws-sdk-cpp uses FindPythonInterp

include(tools/scwx_config.cmake)

scwx_output_dirs_setup()

enable_testing()

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

set_property(DIRECTORY
             APPEND
             PROPERTY CMAKE_CONFIGURE_DEPENDS
             conanfile.py)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_ALL_NO_LIB")

set(SCWX_DIR ${PROJECT_SOURCE_DIR})
set(SCWX_VERSION "0.4.9")

option(SCWX_ADDRESS_SANITIZER "Build with Address Sanitizer" OFF)

add_subdirectory(external)
add_subdirectory(wxdata)
add_subdirectory(scwx-qt)
add_subdirectory(test)

set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER CMakePredefinedTargets)
