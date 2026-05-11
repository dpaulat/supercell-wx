cmake_minimum_required(VERSION 3.24)
set(PROJECT_NAME supercell-wx)

include(tools/scwx_config.cmake)

set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0)

scwx_python_setup()

project(${PROJECT_NAME}
        VERSION      0.5.6
        DESCRIPTION  "Supercell Wx is a free, open source advanced weather radar viewer."
        HOMEPAGE_URL "https://github.com/dpaulat/supercell-wx"
        LANGUAGES    C CXX)

set(CMAKE_POLICY_DEFAULT_CMP0054 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0079 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0148 OLD) # aws-sdk-cpp uses FindPythonInterp

scwx_output_dirs_setup()

enable_testing()

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

set_property(DIRECTORY
             APPEND
             PROPERTY CMAKE_CONFIGURE_DEPENDS
             conanfile.py)

if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_ALL_NO_LIB")

set(SCWX_DIR ${PROJECT_SOURCE_DIR})
set(SCWX_VERSION "0.5.6")

option(SCWX_ADDRESS_SANITIZER "Build with Address Sanitizer" OFF)

add_subdirectory(external)
add_subdirectory(wxdata)
add_subdirectory(scwx-qt)
add_subdirectory(test)

set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER CMakePredefinedTargets)
