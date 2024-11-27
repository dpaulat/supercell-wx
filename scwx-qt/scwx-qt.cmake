cmake_minimum_required(VERSION 3.21)

project(scwx-qt LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost)
find_package(Fontconfig)
find_package(geographiclib)
find_package(geos)
find_package(GLEW)
find_package(glm)
find_package(Python COMPONENTS Interpreter)
find_package(SQLite3)

find_package(QT NAMES Qt6
             COMPONENTS BuildInternals
                        Core
                        Gui
                        LinguistTools
                        Multimedia
                        Network
                        OpenGL
                        OpenGLWidgets
                        Positioning
                        SerialPort
                        Svg
                        Widgets
                        Sql
             REQUIRED)

find_package(Qt${QT_VERSION_MAJOR}
             COMPONENTS BuildInternals
                        Core
                        Gui
                        LinguistTools
                        Multimedia
                        Network
                        OpenGL
                        OpenGLWidgets
                        Positioning
                        SerialPort
                        Svg
                        Widgets
                        Sql
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
set(SRC_EXTERNAL source/scwx/qt/external/stb_image.cpp
                 source/scwx/qt/external/stb_rect_pack.cpp)
set(HDR_GL source/scwx/qt/gl/gl.hpp
           source/scwx/qt/gl/gl_context.hpp
           source/scwx/qt/gl/shader_program.hpp)
set(SRC_GL source/scwx/qt/gl/gl_context.cpp
           source/scwx/qt/gl/shader_program.cpp)
set(HDR_GL_DRAW source/scwx/qt/gl/draw/draw_item.hpp
                source/scwx/qt/gl/draw/geo_icons.hpp
                source/scwx/qt/gl/draw/geo_lines.hpp
                source/scwx/qt/gl/draw/icons.hpp
                source/scwx/qt/gl/draw/linked_vectors.hpp
                source/scwx/qt/gl/draw/placefile_icons.hpp
                source/scwx/qt/gl/draw/placefile_images.hpp
                source/scwx/qt/gl/draw/placefile_lines.hpp
                source/scwx/qt/gl/draw/placefile_polygons.hpp
                source/scwx/qt/gl/draw/placefile_text.hpp
                source/scwx/qt/gl/draw/placefile_triangles.hpp
                source/scwx/qt/gl/draw/rectangle.hpp)
set(SRC_GL_DRAW source/scwx/qt/gl/draw/draw_item.cpp
                source/scwx/qt/gl/draw/geo_icons.cpp
                source/scwx/qt/gl/draw/geo_lines.cpp
                source/scwx/qt/gl/draw/icons.cpp
                source/scwx/qt/gl/draw/linked_vectors.cpp
                source/scwx/qt/gl/draw/placefile_icons.cpp
                source/scwx/qt/gl/draw/placefile_images.cpp
                source/scwx/qt/gl/draw/placefile_lines.cpp
                source/scwx/qt/gl/draw/placefile_polygons.cpp
                source/scwx/qt/gl/draw/placefile_text.cpp
                source/scwx/qt/gl/draw/placefile_triangles.cpp
                source/scwx/qt/gl/draw/rectangle.cpp)
set(HDR_MANAGER source/scwx/qt/manager/alert_manager.hpp
                source/scwx/qt/manager/download_manager.hpp
                source/scwx/qt/manager/font_manager.hpp
                source/scwx/qt/manager/hotkey_manager.hpp
                source/scwx/qt/manager/log_manager.hpp
                source/scwx/qt/manager/media_manager.hpp
                source/scwx/qt/manager/placefile_manager.hpp
                source/scwx/qt/manager/marker_manager.hpp
                source/scwx/qt/manager/position_manager.hpp
                source/scwx/qt/manager/radar_product_manager.hpp
                source/scwx/qt/manager/radar_product_manager_notifier.hpp
                source/scwx/qt/manager/resource_manager.hpp
                source/scwx/qt/manager/settings_manager.hpp
                source/scwx/qt/manager/text_event_manager.hpp
                source/scwx/qt/manager/thread_manager.hpp
                source/scwx/qt/manager/timeline_manager.hpp
                source/scwx/qt/manager/update_manager.hpp)
set(SRC_MANAGER source/scwx/qt/manager/alert_manager.cpp
                source/scwx/qt/manager/download_manager.cpp
                source/scwx/qt/manager/font_manager.cpp
                source/scwx/qt/manager/hotkey_manager.cpp
                source/scwx/qt/manager/log_manager.cpp
                source/scwx/qt/manager/media_manager.cpp
                source/scwx/qt/manager/placefile_manager.cpp
                source/scwx/qt/manager/marker_manager.cpp
                source/scwx/qt/manager/position_manager.cpp
                source/scwx/qt/manager/radar_product_manager.cpp
                source/scwx/qt/manager/radar_product_manager_notifier.cpp
                source/scwx/qt/manager/resource_manager.cpp
                source/scwx/qt/manager/settings_manager.cpp
                source/scwx/qt/manager/text_event_manager.cpp
                source/scwx/qt/manager/thread_manager.cpp
                source/scwx/qt/manager/timeline_manager.cpp
                source/scwx/qt/manager/update_manager.cpp)
set(HDR_MAP source/scwx/qt/map/alert_layer.hpp
            source/scwx/qt/map/color_table_layer.hpp
            source/scwx/qt/map/draw_layer.hpp
            source/scwx/qt/map/generic_layer.hpp
            source/scwx/qt/map/layer_wrapper.hpp
            source/scwx/qt/map/map_context.hpp
            source/scwx/qt/map/map_provider.hpp
            source/scwx/qt/map/map_settings.hpp
            source/scwx/qt/map/map_widget.hpp
            source/scwx/qt/map/overlay_layer.hpp
            source/scwx/qt/map/overlay_product_layer.hpp
            source/scwx/qt/map/placefile_layer.hpp
            source/scwx/qt/map/marker_layer.hpp
            source/scwx/qt/map/radar_product_layer.hpp
            source/scwx/qt/map/radar_range_layer.hpp
            source/scwx/qt/map/radar_site_layer.hpp)
set(SRC_MAP source/scwx/qt/map/alert_layer.cpp
            source/scwx/qt/map/color_table_layer.cpp
            source/scwx/qt/map/draw_layer.cpp
            source/scwx/qt/map/generic_layer.cpp
            source/scwx/qt/map/layer_wrapper.cpp
            source/scwx/qt/map/map_context.cpp
            source/scwx/qt/map/map_provider.cpp
            source/scwx/qt/map/map_widget.cpp
            source/scwx/qt/map/overlay_layer.cpp
            source/scwx/qt/map/overlay_product_layer.cpp
            source/scwx/qt/map/placefile_layer.cpp
            source/scwx/qt/map/marker_layer.cpp
            source/scwx/qt/map/radar_product_layer.cpp
            source/scwx/qt/map/radar_range_layer.cpp
            source/scwx/qt/map/radar_site_layer.cpp)
set(HDR_MODEL source/scwx/qt/model/alert_model.hpp
              source/scwx/qt/model/alert_proxy_model.hpp
              source/scwx/qt/model/imgui_context_model.hpp
              source/scwx/qt/model/layer_model.hpp
              source/scwx/qt/model/placefile_model.hpp
              source/scwx/qt/model/marker_model.hpp
              source/scwx/qt/model/radar_site_model.hpp
              source/scwx/qt/model/tree_item.hpp
              source/scwx/qt/model/tree_model.hpp)
set(SRC_MODEL source/scwx/qt/model/alert_model.cpp
              source/scwx/qt/model/alert_proxy_model.cpp
              source/scwx/qt/model/imgui_context_model.cpp
              source/scwx/qt/model/layer_model.cpp
              source/scwx/qt/model/placefile_model.cpp
              source/scwx/qt/model/marker_model.cpp
              source/scwx/qt/model/radar_site_model.cpp
              source/scwx/qt/model/tree_item.cpp
              source/scwx/qt/model/tree_model.cpp)
set(HDR_REQUEST source/scwx/qt/request/download_request.hpp
                source/scwx/qt/request/nexrad_file_request.hpp)
set(SRC_REQUEST source/scwx/qt/request/download_request.cpp
                source/scwx/qt/request/nexrad_file_request.cpp)
set(HDR_SETTINGS source/scwx/qt/settings/alert_palette_settings.hpp
                 source/scwx/qt/settings/audio_settings.hpp
                 source/scwx/qt/settings/general_settings.hpp
                 source/scwx/qt/settings/hotkey_settings.hpp
                 source/scwx/qt/settings/line_settings.hpp
                 source/scwx/qt/settings/map_settings.hpp
                 source/scwx/qt/settings/palette_settings.hpp
                 source/scwx/qt/settings/product_settings.hpp
                 source/scwx/qt/settings/settings_category.hpp
                 source/scwx/qt/settings/settings_container.hpp
                 source/scwx/qt/settings/settings_definitions.hpp
                 source/scwx/qt/settings/settings_interface.hpp
                 source/scwx/qt/settings/settings_interface_base.hpp
                 source/scwx/qt/settings/settings_variable.hpp
                 source/scwx/qt/settings/settings_variable_base.hpp
                 source/scwx/qt/settings/text_settings.hpp
                 source/scwx/qt/settings/ui_settings.hpp
                 source/scwx/qt/settings/unit_settings.hpp)
set(SRC_SETTINGS source/scwx/qt/settings/alert_palette_settings.cpp
                 source/scwx/qt/settings/audio_settings.cpp
                 source/scwx/qt/settings/general_settings.cpp
                 source/scwx/qt/settings/hotkey_settings.cpp
                 source/scwx/qt/settings/line_settings.cpp
                 source/scwx/qt/settings/map_settings.cpp
                 source/scwx/qt/settings/palette_settings.cpp
                 source/scwx/qt/settings/product_settings.cpp
                 source/scwx/qt/settings/settings_category.cpp
                 source/scwx/qt/settings/settings_container.cpp
                 source/scwx/qt/settings/settings_interface.cpp
                 source/scwx/qt/settings/settings_interface_base.cpp
                 source/scwx/qt/settings/settings_variable.cpp
                 source/scwx/qt/settings/settings_variable_base.cpp
                 source/scwx/qt/settings/text_settings.cpp
                 source/scwx/qt/settings/ui_settings.cpp
                 source/scwx/qt/settings/unit_settings.cpp)
set(HDR_TYPES source/scwx/qt/types/alert_types.hpp
              source/scwx/qt/types/event_types.hpp
              source/scwx/qt/types/font_types.hpp
              source/scwx/qt/types/github_types.hpp
              source/scwx/qt/types/hotkey_types.hpp
              source/scwx/qt/types/icon_types.hpp
              source/scwx/qt/types/imgui_font.hpp
              source/scwx/qt/types/layer_types.hpp
              source/scwx/qt/types/location_types.hpp
              source/scwx/qt/types/map_types.hpp
              source/scwx/qt/types/media_types.hpp
              source/scwx/qt/types/marker_types.hpp
              source/scwx/qt/types/qt_types.hpp
              source/scwx/qt/types/radar_product_record.hpp
              source/scwx/qt/types/text_event_key.hpp
              source/scwx/qt/types/text_types.hpp
              source/scwx/qt/types/texture_types.hpp
              source/scwx/qt/types/time_types.hpp
              source/scwx/qt/types/unit_types.hpp)
set(SRC_TYPES source/scwx/qt/types/alert_types.cpp
              source/scwx/qt/types/github_types.cpp
              source/scwx/qt/types/hotkey_types.cpp
              source/scwx/qt/types/icon_types.cpp
              source/scwx/qt/types/imgui_font.cpp
              source/scwx/qt/types/layer_types.cpp
              source/scwx/qt/types/location_types.cpp
              source/scwx/qt/types/map_types.cpp
              source/scwx/qt/types/media_types.cpp
              source/scwx/qt/types/qt_types.cpp
              source/scwx/qt/types/radar_product_record.cpp
              source/scwx/qt/types/text_event_key.cpp
              source/scwx/qt/types/text_types.cpp
              source/scwx/qt/types/texture_types.cpp
              source/scwx/qt/types/time_types.cpp
              source/scwx/qt/types/unit_types.cpp)
set(HDR_UI source/scwx/qt/ui/about_dialog.hpp
           source/scwx/qt/ui/alert_dialog.hpp
           source/scwx/qt/ui/alert_dock_widget.hpp
           source/scwx/qt/ui/animation_dock_widget.hpp
           source/scwx/qt/ui/collapsible_group.hpp
           source/scwx/qt/ui/county_dialog.hpp
           source/scwx/qt/ui/download_dialog.hpp
           source/scwx/qt/ui/edit_line_dialog.hpp
           source/scwx/qt/ui/flow_layout.hpp
           source/scwx/qt/ui/gps_info_dialog.hpp
           source/scwx/qt/ui/hotkey_edit.hpp
           source/scwx/qt/ui/imgui_debug_dialog.hpp
           source/scwx/qt/ui/imgui_debug_widget.hpp
           source/scwx/qt/ui/layer_dialog.hpp
           source/scwx/qt/ui/left_elided_item_delegate.hpp
           source/scwx/qt/ui/level2_products_widget.hpp
           source/scwx/qt/ui/level2_settings_widget.hpp
           source/scwx/qt/ui/level3_products_widget.hpp
           source/scwx/qt/ui/line_label.hpp
           source/scwx/qt/ui/open_url_dialog.hpp
           source/scwx/qt/ui/placefile_dialog.hpp
           source/scwx/qt/ui/placefile_settings_widget.hpp
           source/scwx/qt/ui/marker_dialog.hpp
           source/scwx/qt/ui/marker_settings_widget.hpp
           source/scwx/qt/ui/progress_dialog.hpp
           source/scwx/qt/ui/radar_site_dialog.hpp
           source/scwx/qt/ui/serial_port_dialog.hpp
           source/scwx/qt/ui/settings_dialog.hpp
           source/scwx/qt/ui/update_dialog.hpp
           source/scwx/qt/ui/wfo_dialog.hpp)
set(SRC_UI source/scwx/qt/ui/about_dialog.cpp
           source/scwx/qt/ui/alert_dialog.cpp
           source/scwx/qt/ui/alert_dock_widget.cpp
           source/scwx/qt/ui/animation_dock_widget.cpp
           source/scwx/qt/ui/collapsible_group.cpp
           source/scwx/qt/ui/county_dialog.cpp
           source/scwx/qt/ui/download_dialog.cpp
           source/scwx/qt/ui/edit_line_dialog.cpp
           source/scwx/qt/ui/flow_layout.cpp
           source/scwx/qt/ui/gps_info_dialog.cpp
           source/scwx/qt/ui/hotkey_edit.cpp
           source/scwx/qt/ui/imgui_debug_dialog.cpp
           source/scwx/qt/ui/imgui_debug_widget.cpp
           source/scwx/qt/ui/layer_dialog.cpp
           source/scwx/qt/ui/left_elided_item_delegate.cpp
           source/scwx/qt/ui/level2_products_widget.cpp
           source/scwx/qt/ui/level2_settings_widget.cpp
           source/scwx/qt/ui/level3_products_widget.cpp
           source/scwx/qt/ui/line_label.cpp
           source/scwx/qt/ui/open_url_dialog.cpp
           source/scwx/qt/ui/placefile_dialog.cpp
           source/scwx/qt/ui/placefile_settings_widget.cpp
           source/scwx/qt/ui/marker_dialog.cpp
           source/scwx/qt/ui/marker_settings_widget.cpp
           source/scwx/qt/ui/progress_dialog.cpp
           source/scwx/qt/ui/radar_site_dialog.cpp
           source/scwx/qt/ui/settings_dialog.cpp
           source/scwx/qt/ui/serial_port_dialog.cpp
           source/scwx/qt/ui/update_dialog.cpp
           source/scwx/qt/ui/wfo_dialog.cpp)
set(UI_UI  source/scwx/qt/ui/about_dialog.ui
           source/scwx/qt/ui/alert_dialog.ui
           source/scwx/qt/ui/alert_dock_widget.ui
           source/scwx/qt/ui/animation_dock_widget.ui
           source/scwx/qt/ui/collapsible_group.ui
           source/scwx/qt/ui/county_dialog.ui
           source/scwx/qt/ui/edit_line_dialog.ui
           source/scwx/qt/ui/gps_info_dialog.ui
           source/scwx/qt/ui/imgui_debug_dialog.ui
           source/scwx/qt/ui/layer_dialog.ui
           source/scwx/qt/ui/open_url_dialog.ui
           source/scwx/qt/ui/placefile_dialog.ui
           source/scwx/qt/ui/placefile_settings_widget.ui
           source/scwx/qt/ui/marker_dialog.ui
           source/scwx/qt/ui/marker_settings_widget.ui
           source/scwx/qt/ui/progress_dialog.ui
           source/scwx/qt/ui/radar_site_dialog.ui
           source/scwx/qt/ui/settings_dialog.ui
           source/scwx/qt/ui/serial_port_dialog.ui
           source/scwx/qt/ui/update_dialog.ui
           source/scwx/qt/ui/wfo_dialog.ui)
set(HDR_UI_SETTINGS source/scwx/qt/ui/settings/alert_palette_settings_widget.hpp
                    source/scwx/qt/ui/settings/hotkey_settings_widget.hpp
                    source/scwx/qt/ui/settings/settings_page_widget.hpp
                    source/scwx/qt/ui/settings/unit_settings_widget.hpp)
set(SRC_UI_SETTINGS source/scwx/qt/ui/settings/alert_palette_settings_widget.cpp
                    source/scwx/qt/ui/settings/hotkey_settings_widget.cpp
                    source/scwx/qt/ui/settings/settings_page_widget.cpp
                    source/scwx/qt/ui/settings/unit_settings_widget.cpp)
set(HDR_UI_SETUP source/scwx/qt/ui/setup/audio_codec_page.hpp
                 source/scwx/qt/ui/setup/finish_page.hpp
                 source/scwx/qt/ui/setup/map_layout_page.hpp
                 source/scwx/qt/ui/setup/map_provider_page.hpp
                 source/scwx/qt/ui/setup/setup_wizard.hpp
                 source/scwx/qt/ui/setup/welcome_page.hpp)
set(SRC_UI_SETUP source/scwx/qt/ui/setup/audio_codec_page.cpp
                 source/scwx/qt/ui/setup/finish_page.cpp
                 source/scwx/qt/ui/setup/map_layout_page.cpp
                 source/scwx/qt/ui/setup/map_provider_page.cpp
                 source/scwx/qt/ui/setup/setup_wizard.cpp
                 source/scwx/qt/ui/setup/welcome_page.cpp)
set(HDR_UTIL source/scwx/qt/util/color.hpp
             source/scwx/qt/util/file.hpp
             source/scwx/qt/util/geographic_lib.hpp
             source/scwx/qt/util/imgui.hpp
             source/scwx/qt/util/json.hpp
             source/scwx/qt/util/maplibre.hpp
             source/scwx/qt/util/network.hpp
             source/scwx/qt/util/streams.hpp
             source/scwx/qt/util/texture_atlas.hpp
             source/scwx/qt/util/q_file_buffer.hpp
             source/scwx/qt/util/q_file_input_stream.hpp
             source/scwx/qt/util/time.hpp
             source/scwx/qt/util/tooltip.hpp)
set(SRC_UTIL source/scwx/qt/util/color.cpp
             source/scwx/qt/util/file.cpp
             source/scwx/qt/util/geographic_lib.cpp
             source/scwx/qt/util/imgui.cpp
             source/scwx/qt/util/json.cpp
             source/scwx/qt/util/maplibre.cpp
             source/scwx/qt/util/network.cpp
             source/scwx/qt/util/texture_atlas.cpp
             source/scwx/qt/util/q_file_buffer.cpp
             source/scwx/qt/util/q_file_input_stream.cpp
             source/scwx/qt/util/time.cpp
             source/scwx/qt/util/tooltip.cpp)
set(HDR_VIEW source/scwx/qt/view/level2_product_view.hpp
             source/scwx/qt/view/level3_product_view.hpp
             source/scwx/qt/view/level3_radial_view.hpp
             source/scwx/qt/view/level3_raster_view.hpp
             source/scwx/qt/view/overlay_product_view.hpp
             source/scwx/qt/view/radar_product_view.hpp
             source/scwx/qt/view/radar_product_view_factory.hpp)
set(SRC_VIEW source/scwx/qt/view/level2_product_view.cpp
             source/scwx/qt/view/level3_product_view.cpp
             source/scwx/qt/view/level3_radial_view.cpp
             source/scwx/qt/view/level3_raster_view.cpp
             source/scwx/qt/view/overlay_product_view.cpp
             source/scwx/qt/view/radar_product_view.cpp
             source/scwx/qt/view/radar_product_view_factory.cpp)

set(RESOURCE_FILES scwx-qt.qrc)

set(SHADER_FILES gl/color.frag
                 gl/color.vert
                 gl/geo_line.vert
                 gl/geo_texture2d.vert
                 gl/map_color.vert
                 gl/radar.frag
                 gl/radar.vert
                 gl/texture1d.frag
                 gl/texture1d.vert
                 gl/texture2d.frag
                 gl/texture2d_array.frag
                 gl/texture2d_array.vert
                 gl/threshold.geom)

set(CMAKE_FILES scwx-qt.cmake)

set(JSON_FILES res/config/radar_sites.json)

set(TS_FILES ts/scwx_en_US.ts)

set(RADAR_SITES_FILE ${scwx-qt_SOURCE_DIR}/res/config/radar_sites.json)
set(COUNTY_DBF_FILES ${SCWX_DIR}/data/db/c_05mr24.dbf)
set(ZONE_DBF_FILES   ${SCWX_DIR}/data/db/fz05mr24.dbf
                     ${SCWX_DIR}/data/db/mz05mr24.dbf
                     ${SCWX_DIR}/data/db/oz05mr24.dbf
                     ${SCWX_DIR}/data/db/z_05mr24.dbf)
set(STATE_DBF_FILES  ${SCWX_DIR}/data/db/s_05mr24.dbf)
set(WFO_DBF_FILES    ${SCWX_DIR}/data/db/w_05mr24.dbf)
set(COUNTIES_SQLITE_DB ${scwx-qt_BINARY_DIR}/res/db/counties.db)

set(RESOURCE_INPUT  ${scwx-qt_SOURCE_DIR}/res/scwx-qt.rc.in)
set(RESOURCE_OUTPUT ${scwx-qt_BINARY_DIR}/res/scwx-qt.rc)
set(VERSIONS_INPUT  ${scwx-qt_SOURCE_DIR}/source/scwx/qt/main/versions.hpp.in)
set(VERSIONS_CACHE  ${scwx-qt_BINARY_DIR}/versions_cache.json)
set(VERSIONS_HEADER ${scwx-qt_BINARY_DIR}/scwx/qt/main/versions.hpp)

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
                    ${HDR_UI_SETTINGS}
                    ${SRC_UI_SETTINGS}
                    ${HDR_UI_SETUP}
                    ${SRC_UI_SETUP}
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

source_group("Header Files\\main"         FILES ${HDR_MAIN})
source_group("Source Files\\main"         FILES ${SRC_MAIN})
source_group("Header Files\\config"       FILES ${HDR_CONFIG})
source_group("Source Files\\config"       FILES ${SRC_CONFIG})
source_group("Source Files\\external"     FILES ${SRC_EXTERNAL})
source_group("Header Files\\gl"           FILES ${HDR_GL})
source_group("Source Files\\gl"           FILES ${SRC_GL})
source_group("Header Files\\gl\\draw"     FILES ${HDR_GL_DRAW})
source_group("Source Files\\gl\\draw"     FILES ${SRC_GL_DRAW})
source_group("Header Files\\manager"      FILES ${HDR_MANAGER})
source_group("Source Files\\manager"      FILES ${SRC_MANAGER})
source_group("UI Files\\main"             FILES ${UI_MAIN})
source_group("Header Files\\map"          FILES ${HDR_MAP})
source_group("Source Files\\map"          FILES ${SRC_MAP})
source_group("Header Files\\model"        FILES ${HDR_MODEL})
source_group("Source Files\\model"        FILES ${SRC_MODEL})
source_group("Header Files\\request"      FILES ${HDR_REQUEST})
source_group("Source Files\\request"      FILES ${SRC_REQUEST})
source_group("Header Files\\settings"     FILES ${HDR_SETTINGS})
source_group("Source Files\\settings"     FILES ${SRC_SETTINGS})
source_group("Header Files\\types"        FILES ${HDR_TYPES})
source_group("Source Files\\types"        FILES ${SRC_TYPES})
source_group("Header Files\\ui"           FILES ${HDR_UI})
source_group("Source Files\\ui"           FILES ${SRC_UI})
source_group("Header Files\\ui\\settings" FILES ${HDR_UI_SETTINGS})
source_group("Source Files\\ui\\settings" FILES ${SRC_UI_SETTINGS})
source_group("Header Files\\ui\\setup"    FILES ${HDR_UI_SETUP})
source_group("Source Files\\ui\\setup"    FILES ${SRC_UI_SETUP})
source_group("UI Files\\ui"               FILES ${UI_UI})
source_group("Header Files\\util"         FILES ${HDR_UTIL})
source_group("Source Files\\util"         FILES ${SRC_UTIL})
source_group("Header Files\\view"         FILES ${HDR_VIEW})
source_group("Source Files\\view"         FILES ${SRC_VIEW})
source_group("OpenGL Shaders"             FILES ${SHADER_FILES})
source_group("Resources"                  FILES ${RESOURCE_FILES})
source_group("Resources\\json"            FILES ${JSON_FILES})
source_group("I18N Files"                 FILES ${TS_FILES})

add_library(scwx-qt OBJECT ${PROJECT_SOURCES})
set_property(TARGET scwx-qt PROPERTY AUTOMOC ON)
set_property(TARGET scwx-qt PROPERTY AUTOGEN_ORIGIN_DEPENDS OFF)

add_custom_command(OUTPUT  ${COUNTIES_SQLITE_DB}
                   COMMAND ${Python_EXECUTABLE}
                           ${scwx-qt_SOURCE_DIR}/tools/generate_counties_db.py
                           -c ${COUNTY_DBF_FILES}
                           -z ${ZONE_DBF_FILES}
                           -s ${STATE_DBF_FILES}
                           -w ${WFO_DBF_FILES}
                           -o ${COUNTIES_SQLITE_DB}
                   DEPENDS ${scwx-qt_SOURCE_DIR}/tools/generate_counties_db.py
                           ${COUNTY_DB_FILES}
                           ${STATE_DBF_FILES}
                           ${ZONE_DBF_FILES}
                           ${WFO_DBF_FILES})

add_custom_target(scwx-qt_generate_counties_db ALL
                  DEPENDS ${COUNTIES_SQLITE_DB})

add_dependencies(scwx-qt scwx-qt_generate_counties_db)

if (DEFINED ENV{GITHUB_RUN_NUMBER})
    set(SCWX_BUILD_NUM $ENV{GITHUB_RUN_NUMBER})
else()
    set(SCWX_BUILD_NUM 0)
endif()

if (WIN32)
    add_custom_command(OUTPUT  ${VERSIONS_HEADER} ${RESOURCE_OUTPUT} ${VERSIONS_HEADER}-ALWAYS_RUN
                       COMMAND ${Python_EXECUTABLE}
                               ${scwx-qt_SOURCE_DIR}/tools/generate_versions.py
                               -g ${SCWX_DIR}
                               -v ${SCWX_VERSION}
                               -c ${VERSIONS_CACHE}
                               -i ${VERSIONS_INPUT}
                               -o ${VERSIONS_HEADER}
                               -b ${SCWX_BUILD_NUM}
                               --input-resource ${RESOURCE_INPUT}
                               --output-resource ${RESOURCE_OUTPUT})
else()
    add_custom_command(OUTPUT  ${VERSIONS_HEADER} ${VERSIONS_HEADER}-ALWAYS_RUN
                       COMMAND ${Python_EXECUTABLE}
                               ${scwx-qt_SOURCE_DIR}/tools/generate_versions.py
                               -g ${SCWX_DIR}
                               -v ${SCWX_VERSION}
                               -c ${VERSIONS_CACHE}
                               -i ${VERSIONS_INPUT}
                               -o ${VERSIONS_HEADER})
endif()

add_custom_target(scwx-qt_generate_versions ALL
                  DEPENDS ${VERSIONS_HEADER})

add_dependencies(scwx-qt scwx-qt_generate_versions)

add_custom_target(scwx-qt_update_radar_sites
                  COMMAND ${Python_EXECUTABLE}
                          ${scwx-qt_SOURCE_DIR}/tools/update_radar_sites.py
                          -u ${RADAR_SITES_FILE}
                          -t -w)

qt_add_resources(scwx-qt "generated"
                 PREFIX  "/"
                 BASE    ${scwx-qt_BINARY_DIR}
                 FILES   ${COUNTIES_SQLITE_DB})

qt_add_translations(scwx-qt TS_FILES ${TS_FILES}
                    INCLUDE_DIRECTORIES true
                    LUPDATE_OPTIONS -locations none -no-ui-lines)

if (TARGET release_translations)
    set_target_properties(release_translations PROPERTIES FOLDER qt)
endif()
if (TARGET scwx-qt_lrelease)
    set_target_properties(scwx-qt_lrelease     PROPERTIES FOLDER qt)
endif()
if (TARGET scwx-qt_lupdate)
    set_target_properties(scwx-qt_lupdate      PROPERTIES FOLDER qt)
endif()
if (TARGET scwx-qt_other_files)
    set_target_properties(scwx-qt_other_files  PROPERTIES FOLDER qt)
endif()
if (TARGET update_translations)
    set_target_properties(update_translations PROPERTIES FOLDER qt)
endif()

set_target_properties(scwx-qt_generate_counties_db PROPERTIES FOLDER generate)
set_target_properties(scwx-qt_generate_versions    PROPERTIES FOLDER generate)
set_target_properties(scwx-qt_update_radar_sites   PROPERTIES FOLDER generate)

if (WIN32)
    set(APP_ICON_RESOURCE_WINDOWS ${RESOURCE_OUTPUT})
    qt_add_executable(supercell-wx ${EXECUTABLE_SOURCES} ${APP_ICON_RESOURCE_WINDOWS})
    set_target_properties(supercell-wx PROPERTIES WIN32_EXECUTABLE $<IF:$<CONFIG:Release>,TRUE,FALSE>)
else()
    qt_add_executable(supercell-wx ${EXECUTABLE_SOURCES})
endif()

if (WIN32)
    target_compile_definitions(scwx-qt      PUBLIC WIN32_LEAN_AND_MEAN)
    target_compile_definitions(supercell-wx PUBLIC WIN32_LEAN_AND_MEAN)
endif()

if (NOT MSVC)
    # Qt emit keyword is incompatible with TBB
    target_compile_definitions(scwx-qt      PRIVATE QT_NO_EMIT)
    target_compile_definitions(supercell-wx PRIVATE QT_NO_EMIT)
endif()

target_include_directories(scwx-qt PUBLIC ${scwx-qt_SOURCE_DIR}/source
                                          ${FTGL_INCLUDE_DIR}
                                          ${IMGUI_INCLUDE_DIRS}
                                          ${MLN_INCLUDE_DIRS}
                                          ${STB_INCLUDE_DIR}
                                          ${TEXTFLOWCPP_INCLUDE_DIR})

target_include_directories(supercell-wx PUBLIC ${scwx-qt_SOURCE_DIR}/source)

target_compile_options(scwx-qt PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)
target_compile_options(supercell-wx PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)

if (MSVC)
    # Don't include Windows macros
    target_compile_options(scwx-qt PRIVATE -DNOMINMAX)
    target_compile_options(supercell-wx PRIVATE -DNOMINMAX)

    # Enable multi-processor compilation
    target_compile_options(scwx-qt PRIVATE "/MP")
    target_compile_options(supercell-wx PRIVATE "/MP")
endif()

# Address Sanitizer options
if (SCWX_ADDRESS_SANITIZER)
    target_compile_options(scwx-qt PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/fsanitize=address /EHsc /D_DISABLE_STRING_ANNOTATION /D_DISABLE_VECTOR_ANNOTATION>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fsanitize=address -fsanitize-recover=address>
    )
    target_compile_options(supercell-wx PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/fsanitize=address /EHsc /D_DISABLE_STRING_ANNOTATION /D_DISABLE_VECTOR_ANNOTATION>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fsanitize=address -fsanitize-recover=address>
    )
    target_link_options(supercell-wx PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fsanitize=address>
    )
endif()

if (MSVC)
    # Produce PDB file for debug
    target_compile_options(scwx-qt PRIVATE "$<$<CONFIG:Release>:/Zi>")
    target_compile_options(supercell-wx PRIVATE "$<$<CONFIG:Release>:/Zi>")
    target_link_options(supercell-wx PRIVATE "$<$<CONFIG:Release>:/DEBUG>")
    target_link_options(supercell-wx PRIVATE "$<$<CONFIG:Release>:/OPT:REF>")
    target_link_options(supercell-wx PRIVATE "$<$<CONFIG:Release>:/OPT:ICF>")
else()
    target_compile_options(scwx-qt PRIVATE "$<$<CONFIG:Release>:-g>")
    target_compile_options(supercell-wx PRIVATE "$<$<CONFIG:Release>:-g>")
endif()

target_link_libraries(scwx-qt PUBLIC Qt${QT_VERSION_MAJOR}::Widgets
                                     Qt${QT_VERSION_MAJOR}::OpenGLWidgets
                                     Qt${QT_VERSION_MAJOR}::Multimedia
                                     Qt${QT_VERSION_MAJOR}::Positioning
                                     Qt${QT_VERSION_MAJOR}::SerialPort
                                     Qt${QT_VERSION_MAJOR}::Svg
                                     Boost::json
                                     Boost::timer
                                     QMapLibre::Core
                                     $<$<CXX_COMPILER_ID:MSVC>:opengl32>
                                     $<$<CXX_COMPILER_ID:MSVC>:SetupAPI>
                                     Fontconfig::Fontconfig
                                     GeographicLib::GeographicLib
                                     GEOS::geos
                                     GEOS::geos_cxx_flags
                                     GLEW::GLEW
                                     glm::glm
                                     imgui
                                     qt6ct-common
                                     SQLite::SQLite3
                                     wxdata)

target_link_libraries(supercell-wx PRIVATE scwx-qt
                                           wxdata)

# Set DT_RUNPATH for Linux targets
set_target_properties(MLNQtCore    PROPERTIES INSTALL_RPATH "\$ORIGIN/../lib") # QMapLibre::Core
set_target_properties(supercell-wx PROPERTIES INSTALL_RPATH "\$ORIGIN/../lib")

install(TARGETS supercell-wx
                MLNQtCore # QMapLibre::Core
        RUNTIME_DEPENDENCIES
          PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-" "qt6"
          POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
                               "^(/usr)?/lib/.*\\.so(\\..*)?"
        RUNTIME
          COMPONENT supercell-wx
        LIBRARY
          COMPONENT supercell-wx
          OPTIONAL)

# NO_TRANSLATIONS is needed for Qt 6.5.0 (will be fixed in 6.5.1)
# https://bugreports.qt.io/browse/QTBUG-112204
qt_generate_deploy_app_script(TARGET MLNQtCore # QMapLibre::Core
                              OUTPUT_SCRIPT deploy_script_qmaplibre_core
                              NO_TRANSLATIONS
                              NO_UNSUPPORTED_PLATFORM_ERROR)

qt_generate_deploy_app_script(TARGET supercell-wx
                              OUTPUT_SCRIPT deploy_script_scwx
                              NO_TRANSLATIONS
                              NO_UNSUPPORTED_PLATFORM_ERROR)

install(SCRIPT ${deploy_script_qmaplibre_core}
        COMPONENT supercell-wx)

install(SCRIPT ${deploy_script_scwx}
        COMPONENT supercell-wx)

if (MSVC)
    set(CPACK_PACKAGE_NAME                "Supercell Wx")
    set(CPACK_PACKAGE_VENDOR              "Dan Paulat")
    set(CPACK_PACKAGE_FILE_NAME           "supercell-wx-v${SCWX_VERSION}-windows-x64")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY   "Supercell Wx")
    set(CPACK_PACKAGE_ICON                "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/scwx-256.ico")
    set(CPACK_PACKAGE_CHECKSUM            SHA256)
    set(CPACK_RESOURCE_FILE_LICENSE       "${SCWX_DIR}/LICENSE.txt")
    set(CPACK_GENERATOR                   WIX)
    set(CPACK_PACKAGE_EXECUTABLES         "supercell-wx;Supercell Wx")
    set(CPACK_WIX_UPGRADE_GUID            36AD0F51-4D4F-4B5D-AB61-94C6B4E4FE1C)
    set(CPACK_WIX_UI_BANNER               "${CMAKE_CURRENT_SOURCE_DIR}/res/images/scwx-banner.png")
    set(CPACK_WIX_UI_DIALOG               "${CMAKE_CURRENT_SOURCE_DIR}/res/images/scwx-dialog.png")
    set(CPACK_WIX_TEMPLATE                "${CMAKE_CURRENT_SOURCE_DIR}/wix.template.in")
    set(CPACK_WIX_EXTENSIONS              WixUIExtension WiXUtilExtension)

    set(CPACK_INSTALL_CMAKE_PROJECTS
        "${CMAKE_CURRENT_BINARY_DIR};${CMAKE_PROJECT_NAME};supercell-wx;/")

    include(CPack)
endif()
