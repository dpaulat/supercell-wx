cmake_minimum_required(VERSION 3.24)
project(scwx-test CXX)

include(GoogleTest)

find_package(Boost)
find_package(BZip2)
find_package(GTest)

set(SRC_MAIN source/scwx/wxtest.cpp)
set(SRC_AWIPS_TESTS source/scwx/awips/coded_location.test.cpp
                    source/scwx/awips/coded_time_motion_location.test.cpp
                    source/scwx/awips/pvtec.test.cpp
                    source/scwx/awips/text_product_file.test.cpp
                    source/scwx/awips/ugc.test.cpp)
set(SRC_COMMON_TESTS source/scwx/common/color_table.test.cpp
                     source/scwx/common/products.test.cpp)
set(SRC_GR_TESTS source/scwx/gr/placefile.test.cpp)
set(SRC_NETWORK_TESTS source/scwx/network/dir_list.test.cpp)
set(SRC_PROVIDER_TESTS source/scwx/provider/aws_level2_data_provider.test.cpp
                       source/scwx/provider/aws_level3_data_provider.test.cpp
                       source/scwx/provider/warnings_provider.test.cpp)
set(SRC_QT_CONFIG_TESTS source/scwx/qt/config/county_database.test.cpp
                        source/scwx/qt/config/radar_site.test.cpp)
set(SRC_QT_MANAGER_TESTS source/scwx/qt/manager/settings_manager.test.cpp
                         source/scwx/qt/manager/update_manager.test.cpp)
set(SRC_QT_MAP_TESTS source/scwx/qt/map/map_provider.test.cpp)
set(SRC_QT_MODEL_TESTS source/scwx/qt/model/imgui_context_model.test.cpp)
set(SRC_QT_SETTINGS_TESTS source/scwx/qt/settings/settings_container.test.cpp
                          source/scwx/qt/settings/settings_variable.test.cpp)
set(SRC_QT_UTIL_TESTS source/scwx/qt/util/q_file_input_stream.test.cpp)
set(SRC_UTIL_TESTS source/scwx/util/float.test.cpp
                   source/scwx/util/rangebuf.test.cpp
                   source/scwx/util/streams.test.cpp
                   source/scwx/util/strings.test.cpp
                   source/scwx/util/vectorbuf.test.cpp)
set(SRC_WSR88D_TESTS source/scwx/wsr88d/ar2v_file.test.cpp
                     source/scwx/wsr88d/level3_file.test.cpp
                     source/scwx/wsr88d/nexrad_file_factory.test.cpp)

set(CMAKE_FILES test.cmake)

add_executable(wxtest ${SRC_MAIN}
                      ${SRC_AWIPS_TESTS}
                      ${SRC_COMMON_TESTS}
                      ${SRC_GR_TESTS}
                      ${SRC_NETWORK_TESTS}
                      ${SRC_PROVIDER_TESTS}
                      ${SRC_QT_CONFIG_TESTS}
                      ${SRC_QT_MANAGER_TESTS}
                      ${SRC_QT_MAP_TESTS}
                      ${SRC_QT_MODEL_TESTS}
                      ${SRC_QT_SETTINGS_TESTS}
                      ${SRC_QT_UTIL_TESTS}
                      ${SRC_UTIL_TESTS}
                      ${SRC_WSR88D_TESTS}
                      ${CMAKE_FILES})

source_group("Source Files\\main"         FILES ${SRC_MAIN})
source_group("Source Files\\awips"        FILES ${SRC_AWIPS_TESTS})
source_group("Source Files\\common"       FILES ${SRC_COMMON_TESTS})
source_group("Source Files\\gr"           FILES ${SRC_GR_TESTS})
source_group("Source Files\\network"      FILES ${SRC_NETWORK_TESTS})
source_group("Source Files\\provider"     FILES ${SRC_PROVIDER_TESTS})
source_group("Source Files\\qt\\config"   FILES ${SRC_QT_CONFIG_TESTS})
source_group("Source Files\\qt\\manager"  FILES ${SRC_QT_MANAGER_TESTS})
source_group("Source Files\\qt\\map"      FILES ${SRC_QT_MAP_TESTS})
source_group("Source Files\\qt\\model"    FILES ${SRC_QT_MODEL_TESTS})
source_group("Source Files\\qt\\settings" FILES ${SRC_QT_SETTINGS_TESTS})
source_group("Source Files\\qt\\util"     FILES ${SRC_QT_UTIL_TESTS})
source_group("Source Files\\util"         FILES ${SRC_UTIL_TESTS})
source_group("Source Files\\wsr88d"       FILES ${SRC_WSR88D_TESTS})

target_include_directories(wxtest PRIVATE ${GTest_INCLUDE_DIRS})

set_target_properties(wxtest PROPERTIES CXX_STANDARD 20
                                        CXX_STANDARD_REQUIRED ON
                                        CXX_EXTENSIONS OFF)

if (MSVC)
    set_target_properties(wxtest PROPERTIES LINK_FLAGS "/ignore:4099")
endif()

target_compile_definitions(wxtest PRIVATE SCWX_TEST_DATA_DIR="${SCWX_DIR}/test/data")

if (MSVC)
    # Don't include Windows macros
    target_compile_options(wxtest PRIVATE -DNOMINMAX)

    # Enable multi-processor compilation
    target_compile_options(wxtest PRIVATE "/MP")
endif()

gtest_discover_tests(wxtest)

target_link_libraries(wxtest GTest::gtest
                             scwx-qt
                             wxdata)
