cmake_minimum_required(VERSION 3.11)
project(scwx-test CXX)

include(GoogleTest)

find_package(Boost)
find_package(BZip2)
find_package(GTest)

set(SRC_MAIN source/scwx/wxtest.cpp)
set(SRC_AWIPS_TESTS source/scwx/awips/text_product_file.test.cpp)
set(SRC_COMMON_TESTS source/scwx/common/color_table.test.cpp)
set(SRC_QT_MANAGER_TESTS source/scwx/qt/manager/settings_manager.test.cpp)
set(SRC_UTIL_TESTS source/scwx/util/rangebuf.test.cpp
                   source/scwx/util/streams.test.cpp
                   source/scwx/util/vectorbuf.test.cpp)
set(SRC_WSR88D_TESTS source/scwx/wsr88d/ar2v_file.test.cpp
                     source/scwx/wsr88d/level3_file.test.cpp)

add_executable(wxtest ${SRC_MAIN}
                      ${SRC_AWIPS_TESTS}
                      ${SRC_COMMON_TESTS}
                      ${SRC_QT_MANAGER_TESTS}
                      ${SRC_UTIL_TESTS}
                      ${SRC_WSR88D_TESTS})

source_group("Source Files\\main"        FILES ${SRC_MAIN})
source_group("Source Files\\awips"       FILES ${SRC_AWIPS_TESTS})
source_group("Source Files\\common"      FILES ${SRC_COMMON_TESTS})
source_group("Source Files\\qt\\manager" FILES ${SRC_QT_MANAGER_TESTS})
source_group("Source Files\\util"        FILES ${SRC_UTIL_TESTS})
source_group("Source Files\\wsr88d"      FILES ${SRC_WSR88D_TESTS})

target_include_directories(wxtest PRIVATE ${GTest_INCLUDE_DIRS})

set_target_properties(wxtest PROPERTIES CXX_STANDARD 20
                                        CXX_STANDARD_REQUIRED ON
                                        CXX_EXTENSIONS OFF)

if (MSVC)
    set_target_properties(wxtest PROPERTIES LINK_FLAGS "/ignore:4099")
endif()

target_compile_definitions(wxtest PRIVATE SCWX_TEST_DATA_DIR="${SCWX_DIR}/test/data")

gtest_discover_tests(wxtest)

target_link_libraries(wxtest GTest::gtest
                             scwx-qt
                             wxdata)
