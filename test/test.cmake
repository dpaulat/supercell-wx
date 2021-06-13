cmake_minimum_required(VERSION 3.11)
project(scwx-test CXX)

include(GoogleTest)

find_package(Boost)
find_package(BZip2)
find_package(GTest)

set(SRC_MAIN source/scwx/wxtest.cpp)
set(SRC_UTIL_TESTS source/scwx/util/rangebuf.test.cpp)
set(SRC_WSR88D_TESTS source/scwx/wsr88d/ar2v_file.test.cpp)

add_executable(wxtest ${SRC_MAIN}
                      ${SRC_UTIL_TESTS}
                      ${SRC_WSR88D_TESTS})

source_group("Source Files\\main"   FILES ${SRC_MAIN})
source_group("Source Files\\util"   FILES ${SRC_UTIL_TESTS})
source_group("Source Files\\wsr88d" FILES ${SRC_WSR88D_TESTS})

target_include_directories(wxtest PRIVATE ${GTest_INCLUDE_DIRS})

set_target_properties(wxtest PROPERTIES CXX_STANDARD 17
                                        CXX_STANDARD_REQUIRED ON
                                        CXX_EXTENSIONS OFF)

if (MSVC)
    set_target_properties(wxtest PROPERTIES LINK_FLAGS "/ignore:4099")
endif()

target_compile_definitions(wxtest PRIVATE SCWX_TEST_DATA_DIR="${SCWX_DIR}/test/data")

gtest_discover_tests(wxtest)

target_link_libraries(wxtest Boost::iostreams
                             Boost::log
                             BZip2::BZip2
                             GTest::gtest
                             wxdata)

if (WIN32)
    target_link_libraries(wxtest Ws2_32)
endif()
