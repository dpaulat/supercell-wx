cmake_minimum_required(VERSION 3.20)

project(scwx-qt LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost)
find_package(Freetype)
find_package(geographiclib)
find_package(glm)
find_package(Python COMPONENTS Interpreter)
find_package(SQLite3)

find_package(QT NAMES Qt6
             COMPONENTS Gui
                        LinguistTools
                        Network
                        OpenGL
                        OpenGLWidgets
                        Widgets REQUIRED)

find_package(Qt${QT_VERSION_MAJOR}
             COMPONENTS Gui
                        LinguistTools
                        Network
                        OpenGL
                        OpenGLWidgets
                        Widgets
             REQUIRED)

set(SRC_EXE_MAIN source/scwx/qt/main/main.cpp)

set(HDR_MAIN source/scwx/qt/main/application.hpp
             source/scwx/qt/main/main_window.hpp)
set(SRC_MAIN source/scwx/qt/main/application.cpp
             source/scwx/qt/main/main_window.cpp)
set(UI_MAIN  source/scwx/qt/main/main_window.ui)
set(HDR_CONFIG source/scwx/qt/config/county_database.hpp
               source/scwx/qt/config/radar_site.hpp)
set(SRC_CONFIG source/scwx/qt/config/county_database.cpp
               source/scwx/qt/config/radar_site.cpp)
set(SRC_EXTERNAL source/scwx/qt/external/stb_rect_pack.cpp)
set(HDR_GL source/scwx/qt/gl/gl.hpp
           source/scwx/qt/gl/gl_context.hpp
           source/scwx/qt/gl/shader_program.hpp
           source/scwx/qt/gl/text_shader.hpp)
set(SRC_GL source/scwx/qt/gl/gl_context.cpp
           source/scwx/qt/gl/shader_program.cpp
           source/scwx/qt/gl/text_shader.cpp)
set(HDR_GL_DRAW source/scwx/qt/gl/draw/draw_item.hpp
                source/scwx/qt/gl/draw/geo_line.hpp
                source/scwx/qt/gl/draw/rectangle.hpp)
set(SRC_GL_DRAW source/scwx/qt/gl/draw/draw_item.cpp
                source/scwx/qt/gl/draw/geo_line.cpp
                source/scwx/qt/gl/draw/rectangle.cpp)
set(HDR_MANAGER source/scwx/qt/manager/radar_product_manager.hpp
                source/scwx/qt/manager/radar_product_manager_notifier.hpp
                source/scwx/qt/manager/resource_manager.hpp
                source/scwx/qt/manager/settings_manager.hpp
                source/scwx/qt/manager/text_event_manager.hpp)
set(SRC_MANAGER source/scwx/qt/manager/radar_product_manager.cpp
                source/scwx/qt/manager/radar_product_manager_notifier.cpp
                source/scwx/qt/manager/resource_manager.cpp
                source/scwx/qt/manager/settings_manager.cpp
                source/scwx/qt/manager/text_event_manager.cpp)
set(HDR_MAP source/scwx/qt/map/alert_layer.hpp
            source/scwx/qt/map/color_table_layer.hpp
            source/scwx/qt/map/draw_layer.hpp
            source/scwx/qt/map/generic_layer.hpp
            source/scwx/qt/map/layer_wrapper.hpp
            source/scwx/qt/map/map_context.hpp
            source/scwx/qt/map/map_settings.hpp
            source/scwx/qt/map/map_widget.hpp
            source/scwx/qt/map/overlay_layer.hpp
            source/scwx/qt/map/radar_product_layer.hpp
            source/scwx/qt/map/radar_range_layer.hpp)
set(SRC_MAP source/scwx/qt/map/alert_layer.cpp
            source/scwx/qt/map/color_table_layer.cpp
            source/scwx/qt/map/draw_layer.cpp
            source/scwx/qt/map/generic_layer.cpp
            source/scwx/qt/map/layer_wrapper.cpp
            source/scwx/qt/map/map_context.cpp
            source/scwx/qt/map/map_widget.cpp
            source/scwx/qt/map/overlay_layer.cpp
            source/scwx/qt/map/radar_product_layer.cpp
            source/scwx/qt/map/radar_range_layer.cpp)
set(HDR_MODEL source/scwx/qt/model/alert_model.hpp
              source/scwx/qt/model/alert_proxy_model.hpp
              source/scwx/qt/model/imgui_context_model.hpp
              source/scwx/qt/model/radar_product_model.hpp
              source/scwx/qt/model/radar_site_model.hpp
              source/scwx/qt/model/tree_item.hpp
              source/scwx/qt/model/tree_model.hpp)
set(SRC_MODEL source/scwx/qt/model/alert_model.cpp
              source/scwx/qt/model/alert_proxy_model.cpp
              source/scwx/qt/model/imgui_context_model.cpp
              source/scwx/qt/model/radar_product_model.cpp
              source/scwx/qt/model/radar_site_model.cpp
              source/scwx/qt/model/tree_item.cpp
              source/scwx/qt/model/tree_model.cpp)
set(HDR_REQUEST source/scwx/qt/request/nexrad_file_request.hpp)
set(SRC_REQUEST source/scwx/qt/request/nexrad_file_request.cpp)
set(HDR_SETTINGS source/scwx/qt/settings/general_settings.hpp
                 source/scwx/qt/settings/map_settings.hpp
                 source/scwx/qt/settings/palette_settings.hpp
                 source/scwx/qt/settings/settings_category.hpp
                 source/scwx/qt/settings/settings_container.hpp
                 source/scwx/qt/settings/settings_variable.hpp
                 source/scwx/qt/settings/settings_variable_base.hpp)
set(SRC_SETTINGS source/scwx/qt/settings/general_settings.cpp
                 source/scwx/qt/settings/map_settings.cpp
                 source/scwx/qt/settings/palette_settings.cpp
                 source/scwx/qt/settings/settings_category.cpp
                 source/scwx/qt/settings/settings_container.cpp
                 source/scwx/qt/settings/settings_variable.cpp
                 source/scwx/qt/settings/settings_variable_base.cpp)
set(HDR_TYPES source/scwx/qt/types/qt_types.hpp
              source/scwx/qt/types/radar_product_record.hpp
              source/scwx/qt/types/text_event_key.hpp)
set(SRC_TYPES source/scwx/qt/types/radar_product_record.cpp
              source/scwx/qt/types/text_event_key.cpp)
set(HDR_UI source/scwx/qt/ui/alert_dialog.hpp
           source/scwx/qt/ui/alert_dock_widget.hpp
           source/scwx/qt/ui/flow_layout.hpp
           source/scwx/qt/ui/imgui_debug_dialog.hpp
           source/scwx/qt/ui/imgui_debug_widget.hpp
           source/scwx/qt/ui/level2_products_widget.hpp
           source/scwx/qt/ui/level2_settings_widget.hpp
           source/scwx/qt/ui/level3_products_widget.hpp
           source/scwx/qt/ui/radar_site_dialog.hpp
           source/scwx/qt/ui/settings_dialog.hpp)
set(SRC_UI source/scwx/qt/ui/alert_dialog.cpp
           source/scwx/qt/ui/alert_dock_widget.cpp
           source/scwx/qt/ui/flow_layout.cpp
           source/scwx/qt/ui/imgui_debug_dialog.cpp
           source/scwx/qt/ui/imgui_debug_widget.cpp
           source/scwx/qt/ui/level2_products_widget.cpp
           source/scwx/qt/ui/level2_settings_widget.cpp
           source/scwx/qt/ui/level3_products_widget.cpp
           source/scwx/qt/ui/radar_site_dialog.cpp
           source/scwx/qt/ui/settings_dialog.cpp)
set(UI_UI  source/scwx/qt/ui/alert_dialog.ui
           source/scwx/qt/ui/alert_dock_widget.ui
           source/scwx/qt/ui/imgui_debug_dialog.ui
           source/scwx/qt/ui/radar_site_dialog.ui
           source/scwx/qt/ui/settings_dialog.ui)
set(HDR_UTIL source/scwx/qt/util/font.hpp
             source/scwx/qt/util/font_buffer.hpp
             source/scwx/qt/util/json.hpp
             source/scwx/qt/util/streams.hpp
             source/scwx/qt/util/texture_atlas.hpp)
set(SRC_UTIL source/scwx/qt/util/font.cpp
             source/scwx/qt/util/font_buffer.cpp
             source/scwx/qt/util/json.cpp
             source/scwx/qt/util/texture_atlas.cpp)
set(HDR_VIEW source/scwx/qt/view/level2_product_view.hpp
             source/scwx/qt/view/level3_product_view.hpp
             source/scwx/qt/view/level3_radial_view.hpp
             source/scwx/qt/view/level3_raster_view.hpp
             source/scwx/qt/view/radar_product_view.hpp
             source/scwx/qt/view/radar_product_view_factory.hpp)
set(SRC_VIEW source/scwx/qt/view/level2_product_view.cpp
             source/scwx/qt/view/level3_product_view.cpp
             source/scwx/qt/view/level3_radial_view.cpp
             source/scwx/qt/view/level3_raster_view.cpp
             source/scwx/qt/view/radar_product_view.cpp
             source/scwx/qt/view/radar_product_view_factory.cpp)

set(RESOURCE_FILES scwx-qt.qrc)

set(SHADER_FILES gl/color.frag
                 gl/color.vert
                 gl/geo_line.vert
                 gl/radar.frag
                 gl/radar.vert
                 gl/text.frag
                 gl/text.vert
                 gl/texture1d.frag
                 gl/texture1d.vert
                 gl/texture2d.frag)

set(CMAKE_FILES scwx-qt.cmake)

set(JSON_FILES res/config/radar_sites.json)

set(TS_FILES ts/scwx_en_US.ts)

set(COUNTY_DBF_FILES ${SCWX_DIR}/data/db/c_13se22.dbf)
set(ZONE_DBF_FILES   ${SCWX_DIR}/data/db/fz13se22.dbf
                     ${SCWX_DIR}/data/db/mz13se22.dbf
                     ${SCWX_DIR}/data/db/oz22mr22.dbf
                     ${SCWX_DIR}/data/db/z_13se22.dbf)
set(COUNTIES_SQLITE_DB ${scwx-qt_BINARY_DIR}/res/db/counties.db)

set(PROJECT_SOURCES ${HDR_MAIN}
                    ${SRC_MAIN}
                    ${HDR_CONFIG}
                    ${SRC_CONFIG}
                    ${SRC_EXTERNAL}
                    ${HDR_GL}
                    ${SRC_GL}
                    ${HDR_GL_DRAW}
                    ${SRC_GL_DRAW}
                    ${HDR_MANAGER}
                    ${SRC_MANAGER}
                    ${UI_MAIN}
                    ${HDR_MAP}
                    ${SRC_MAP}
                    ${HDR_MODEL}
                    ${SRC_MODEL}
                    ${HDR_REQUEST}
                    ${SRC_REQUEST}
                    ${HDR_SETTINGS}
                    ${SRC_SETTINGS}
                    ${HDR_TYPES}
                    ${SRC_TYPES}
                    ${HDR_UI}
                    ${SRC_UI}
                    ${UI_UI}
                    ${HDR_UTIL}
                    ${SRC_UTIL}
                    ${HDR_VIEW}
                    ${SRC_VIEW}
                    ${SHADER_FILES}
                    ${JSON_FILES}
                    ${RESOURCE_FILES}
                    ${TS_FILES}
                    ${CMAKE_FILES})
set(EXECUTABLE_SOURCES ${SRC_EXE_MAIN})

source_group("Header Files\\main"     FILES ${HDR_MAIN})
source_group("Source Files\\main"     FILES ${SRC_MAIN})
source_group("Header Files\\config"   FILES ${HDR_CONFIG})
source_group("Source Files\\config"   FILES ${SRC_CONFIG})
source_group("Source Files\\external" FILES ${SRC_EXTERNAL})
source_group("Header Files\\gl"       FILES ${HDR_GL})
source_group("Source Files\\gl"       FILES ${SRC_GL})
source_group("Header Files\\gl\\draw" FILES ${HDR_GL_DRAW})
source_group("Source Files\\gl\\draw" FILES ${SRC_GL_DRAW})
source_group("Header Files\\manager"  FILES ${HDR_MANAGER})
source_group("Source Files\\manager"  FILES ${SRC_MANAGER})
source_group("UI Files\\main"         FILES ${UI_MAIN})
source_group("Header Files\\map"      FILES ${HDR_MAP})
source_group("Source Files\\map"      FILES ${SRC_MAP})
source_group("Header Files\\model"    FILES ${HDR_MODEL})
source_group("Source Files\\model"    FILES ${SRC_MODEL})
source_group("Header Files\\request"  FILES ${HDR_REQUEST})
source_group("Source Files\\request"  FILES ${SRC_REQUEST})
source_group("Header Files\\settings" FILES ${HDR_SETTINGS})
source_group("Source Files\\settings" FILES ${SRC_SETTINGS})
source_group("Header Files\\types"    FILES ${HDR_TYPES})
source_group("Source Files\\types"    FILES ${SRC_TYPES})
source_group("Header Files\\ui"       FILES ${HDR_UI})
source_group("Source Files\\ui"       FILES ${SRC_UI})
source_group("UI Files\\ui"           FILES ${UI_UI})
source_group("Header Files\\util"     FILES ${HDR_UTIL})
source_group("Source Files\\util"     FILES ${SRC_UTIL})
source_group("Header Files\\view"     FILES ${HDR_VIEW})
source_group("Source Files\\view"     FILES ${SRC_VIEW})
source_group("OpenGL Shaders"         FILES ${SHADER_FILES})
source_group("Resources"              FILES ${RESOURCE_FILES})
source_group("Resources\\json"        FILES ${JSON_FILES})
source_group("I18N Files"             FILES ${TS_FILES})

add_library(scwx-qt OBJECT ${PROJECT_SOURCES})
set_property(TARGET scwx-qt PROPERTY AUTOMOC ON)

add_custom_command(OUTPUT  ${COUNTIES_SQLITE_DB}
                   COMMAND ${Python_EXECUTABLE}
                           ${scwx-qt_SOURCE_DIR}/tools/generate_counties_db.py
                           -c ${COUNTY_DBF_FILES}
                           -z ${ZONE_DBF_FILES}
                           -o ${COUNTIES_SQLITE_DB}
                   DEPENDS ${COUNTY_DB_FILES} ${ZONE_DBF_FILES})

add_custom_target(scwx-qt_generate_counties_db ALL
                  DEPENDS ${COUNTIES_SQLITE_DB})

add_dependencies(scwx-qt scwx-qt_generate_counties_db)

qt_add_resources(scwx-qt "generated"
                 PREFIX  "/"
                 BASE    ${scwx-qt_BINARY_DIR}
                 FILES   ${COUNTIES_SQLITE_DB})

qt_add_translations(scwx-qt TS_FILES ${TS_FILES}
                    INCLUDE_DIRECTORIES true
                    LUPDATE_OPTIONS -locations none -no-ui-lines)

set_target_properties(release_translations PROPERTIES FOLDER qt)
set_target_properties(scwx-qt_lrelease     PROPERTIES FOLDER qt)
set_target_properties(scwx-qt_lupdate      PROPERTIES FOLDER qt)
set_target_properties(scwx-qt_other_files  PROPERTIES FOLDER qt)
set_target_properties(update_translations  PROPERTIES FOLDER qt)

set_target_properties(scwx-qt_generate_counties_db PROPERTIES FOLDER generate)

qt_add_executable(supercell-wx ${EXECUTABLE_SOURCES})

if (WIN32)
    target_compile_definitions(scwx-qt      PUBLIC WIN32_LEAN_AND_MEAN)
    target_compile_definitions(supercell-wx PUBLIC WIN32_LEAN_AND_MEAN)
endif()

target_include_directories(scwx-qt PUBLIC ${scwx-qt_SOURCE_DIR}/source
                                          ${FTGL_INCLUDE_DIR}
                                          ${IMGUI_INCLUDE_DIRS}
                                          ${MBGL_INCLUDE_DIR}
                                          ${STB_INCLUDE_DIR})

target_include_directories(supercell-wx PUBLIC ${scwx-qt_SOURCE_DIR}/source)

target_compile_options(scwx-qt PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)
target_compile_options(supercell-wx PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)

target_link_libraries(scwx-qt PUBLIC Qt${QT_VERSION_MAJOR}::Widgets
                                     Qt${QT_VERSION_MAJOR}::OpenGLWidgets
                                     Boost::json
                                     Boost::timer
                                     qmaplibregl
                                     opengl32
                                     freetype-gl
                                     GeographicLib::GeographicLib
                                     glm::glm
                                     imgui
                                     SQLite::SQLite3
                                     wxdata)

target_link_libraries(supercell-wx PRIVATE scwx-qt
                                           wxdata)
