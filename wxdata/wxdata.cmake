project(scwx-data)

find_package(Boost)

set(HDR_UTIL include/scwx/util/rangebuf.hpp)
set(SRC_UTIL source/scwx/util/rangebuf.cpp)
set(HDR_WSR88D_RPG include/scwx/wsr88d/rpg/ar2v_file.hpp)
set(SRC_WSR88D_RPG source/scwx/wsr88d/rpg/ar2v_file.cpp)
set(HDR_WSR88D_RDA include/scwx/wsr88d/rda/message_header.hpp)
set(SRC_WSR88D_RDA source/scwx/wsr88d/rda/message_header.cpp)

add_library(wxdata OBJECT ${HDR_UTIL}
                          ${SRC_UTIL}
                          ${HDR_WSR88D_RDA}
                          ${SRC_WSR88D_RDA}
                          ${HDR_WSR88D_RPG}
                          ${SRC_WSR88D_RPG})

source_group("Header Files\\util"        FILES ${HDR_UTIL})
source_group("Source Files\\util"        FILES ${SRC_UTIL})
source_group("Header Files\\wsr88d\\rda" FILES ${HDR_WSR88D_RDA})
source_group("Source Files\\wsr88d\\rda" FILES ${SRC_WSR88D_RDA})
source_group("Header Files\\wsr88d\\rpg" FILES ${HDR_WSR88D_RPG})
source_group("Source Files\\wsr88d\\rpg" FILES ${SRC_WSR88D_RPG})

target_include_directories(wxdata PRIVATE ${Boost_INCLUDE_DIR}
                                          ${scwx-data_SOURCE_DIR}/include
                                          ${scwx-data_SOURCE_DIR}/source)
target_include_directories(wxdata INTERFACE ${scwx-data_SOURCE_DIR}/include)

if(MSVC)
    target_compile_options(wxdata PRIVATE /W3)
endif()

set_target_properties(wxdata PROPERTIES CXX_STANDARD 17
                                        CXX_STANDARD_REQUIRED ON
                                        CXX_EXTENSIONS OFF)
