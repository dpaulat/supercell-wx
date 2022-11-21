cmake_minimum_required(VERSION 3.20)
set(PROJECT_NAME scwx-imgui)

set(IMGUI_SOURCES imgui/imconfig.h
                  imgui/imgui.cpp
                  imgui/imgui.h
                  imgui/imgui_demo.cpp
                  imgui/imgui_draw.cpp
                  imgui/imgui_internal.h
                  imgui/imgui_tables.cpp
                  imgui/imgui_widgets.cpp
                  imgui/imstb_rectpack.h
                  imgui/imstb_textedit.h
                  imgui/imstb_truetype.h
                  imgui/backends/imgui_impl_opengl3.cpp
                  imgui/backends/imgui_impl_opengl3.h)

add_library(imgui STATIC ${IMGUI_SOURCES})

target_include_directories(imgui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

set(IMGUI_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui PARENT_SCOPE)

set_target_properties(imgui PROPERTIES FOLDER imgui)
