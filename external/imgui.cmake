cmake_minimum_required(VERSION 3.24)
set(PROJECT_NAME scwx-imgui)

find_package(QT NAMES Qt6
             COMPONENTS Gui
                        Widgets
             REQUIRED)
find_package(Qt${QT_VERSION_MAJOR}
             COMPONENTS Gui
                        Widgets
             REQUIRED)

find_package(Freetype)

set(IMGUI_SOURCES include/scwx/external/imgui/imconfig.h
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
                  imgui/misc/freetype/imgui_freetype.cpp
                  imgui/misc/freetype/imgui_freetype.h
                  imgui-backend-qt/backends/imgui_impl_qt.cpp
                  imgui-backend-qt/backends/imgui_impl_qt.hpp)

if (APPLE)
    enable_language(OBJCXX)
    list(APPEND IMGUI_SOURCES
         imgui/backends/imgui_impl_metal.mm
         imgui/backends/imgui_impl_metal.h)
else()
    find_package(Vulkan REQUIRED)
    list(APPEND IMGUI_SOURCES
         imgui/backends/imgui_impl_vulkan.cpp
         imgui/backends/imgui_impl_vulkan.h)
endif()

add_library(imgui STATIC ${IMGUI_SOURCES})

target_include_directories(imgui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(imgui PUBLIC IMGUI_USER_CONFIG=<scwx/external/imgui/imconfig.h>)

target_link_libraries(imgui PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
                                    Freetype::Freetype)

if (APPLE)
    target_link_libraries(imgui PRIVATE
                          "-framework Metal"
                          "-framework MetalKit"
                          "-framework QuartzCore"
                          "-framework AppKit")
    set_source_files_properties(imgui/backends/imgui_impl_metal.mm
                                PROPERTIES COMPILE_FLAGS "-fobjc-arc")
else()
    target_link_libraries(imgui PRIVATE Vulkan::Vulkan)
endif()

if (MSVC)
    # Produce PDB file for debug
    target_compile_options(imgui PRIVATE "$<$<CONFIG:Release>:/Zi>")
else()
    target_compile_options(imgui PRIVATE "$<$<CONFIG:Release>:-g>")
endif()

set(IMGUI_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/imgui
                       ${CMAKE_CURRENT_SOURCE_DIR}/imgui-backend-qt
    PARENT_SCOPE)

set_target_properties(imgui PROPERTIES FOLDER imgui)
