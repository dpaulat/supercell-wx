cmake_minimum_required(VERSION 3.24)
set(PROJECT_NAME scwx-aws-sdk-cpp)

set(AWS_SDK_WARNINGS_ARE_ERRORS OFF)
set(BUILD_ONLY         "s3")
set(BUILD_SHARED_LIBS  OFF)
set(CPP_STANDARD       17)
set(ENABLE_TESTING     OFF)
set(ENABLE_UNITY_BUILD ON)
set(MINIMIZE_SIZE      OFF)

# Some variables also need set in the cache... set them all!
set(AWS_SDK_WARNINGS_ARE_ERRORS OFF CACHE BOOL "Compiler warning is treated as an error. Try turning this off when observing errors on a new or uncommon compiler")
set(BUILD_ONLY         "s3" CACHE STRING "A semi-colon delimited list of the projects to build")
set(BUILD_SHARED_LIBS  OFF  CACHE BOOL   "If enabled, all aws sdk libraries will be build as shared objects; otherwise all Aws libraries will be built as static objects")
set(CPP_STANDARD       "17" CACHE STRING "Flag to upgrade the C++ standard used. The default is 11. The minimum is 11.")
set(ENABLE_TESTING     OFF  CACHE BOOL   "Flag to enable/disable building unit and integration tests")
set(ENABLE_UNITY_BUILD ON   CACHE BOOL   "If enabled, the SDK will be built using a single unified .cpp file for each service library.  Reduces the size of static library binaries on Windows and Linux")
set(MINIMIZE_SIZE      OFF  CACHE BOOL   "If enabled, the SDK will be built via a unity aggregation process that results in smaller static libraries; additionally, release binaries will favor size optimizations over speed")

# Save off ${CMAKE_CXX_FLAGS} before modifying compiler settings
set(CMAKE_CXX_FLAGS_PREV "${CMAKE_CXX_FLAGS}")

# Fix CMake errors for internal variables not set
include(aws-sdk-cpp/cmake/compiler_settings.cmake)
set_msvc_warnings()

add_subdirectory(aws-sdk-cpp)

# Restore ${CMAKE_CXX_FLAGS} now that aws-sdk-cpp has been added
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_PREV}")

set_target_properties(uninstall         PROPERTIES EXCLUDE_FROM_ALL True)

set_target_properties(aws-c-auth         PROPERTIES FOLDER aws-sdk-cpp)
set_target_properties(aws-c-cal          PROPERTIES FOLDER aws-sdk-cpp)
set_target_properties(aws-c-common       PROPERTIES FOLDER aws-sdk-cpp)
set_target_properties(aws-c-compression  PROPERTIES FOLDER aws-sdk-cpp)
set_target_properties(aws-c-event-stream PROPERTIES FOLDER aws-sdk-cpp)
set_target_properties(aws-checksums      PROPERTIES FOLDER aws-sdk-cpp)
set_target_properties(aws-c-http         PROPERTIES FOLDER aws-sdk-cpp)
set_target_properties(aws-c-io           PROPERTIES FOLDER aws-sdk-cpp)
set_target_properties(aws-c-mqtt         PROPERTIES FOLDER aws-sdk-cpp)
set_target_properties(aws-cpp-sdk-core   PROPERTIES FOLDER aws-sdk-cpp)
set_target_properties(aws-cpp-sdk-s3     PROPERTIES FOLDER aws-sdk-cpp)
set_target_properties(aws-crt-cpp        PROPERTIES FOLDER aws-sdk-cpp)
set_target_properties(aws-c-s3           PROPERTIES FOLDER aws-sdk-cpp)
set_target_properties(aws-c-sdkutils     PROPERTIES FOLDER aws-sdk-cpp)
set_target_properties(uninstall          PROPERTIES FOLDER aws-sdk-cpp/exclude)
