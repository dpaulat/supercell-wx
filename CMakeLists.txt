cmake_minimum_required(VERSION 3.21)
set(PROJECT_NAME supercell-wx)
project(${PROJECT_NAME}
        VERSION      0.4.7
        DESCRIPTION  "Supercell Wx is a free, open source advanced weather radar viewer."
        HOMEPAGE_URL "https://github.com/dpaulat/supercell-wx"
        LANGUAGES    C CXX)

set(CMAKE_POLICY_DEFAULT_CMP0054 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0079 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0148 OLD) # aws-sdk-cpp uses FindPythonInterp

enable_testing()

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

include(${PROJECT_SOURCE_DIR}/external/cmake-conan/conan.cmake)

set_property(DIRECTORY
             APPEND
             PROPERTY CMAKE_CONFIGURE_DEPENDS
             conanfile.py)

# Don't use RelWithDebInfo Conan packages
if (${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
    set(conan_build_type "Release")
else()
    set(conan_build_type ${CMAKE_BUILD_TYPE})
endif()

conan_cmake_autodetect(settings
                       BUILD_TYPE ${conan_build_type})

conan_cmake_install(PATH_OR_REFERENCE ${PROJECT_SOURCE_DIR}
                    BUILD missing
                    REMOTE conancenter
                    SETTINGS ${settings})

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
include(${CMAKE_BINARY_DIR}/conan_paths.cmake)
conan_basic_setup(TARGETS)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_ALL_NO_LIB")

set(SCWX_DIR ${PROJECT_SOURCE_DIR})
set(SCWX_VERSION "0.4.7")

option(SCWX_ADDRESS_SANITIZER "Build with Address Sanitizer" OFF)

add_subdirectory(external)
add_subdirectory(wxdata)
add_subdirectory(scwx-qt)
add_subdirectory(test)

set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER CMakePredefinedTargets)
