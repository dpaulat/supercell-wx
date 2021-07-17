cmake_minimum_required(VERSION 3.11)

project(scwx-qt LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost)
find_package(glm)
find_package(proj)

# QtCreator supports the following variables for Android, which are identical to qmake Android variables.
# Check https://doc.qt.io/qt/deployment-android.html for more information.
# They need to be set before the find_package( ...) calls below.

#if(ANDROID)
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
#    if (ANDROID_ABI STREQUAL "armeabi-v7a")
#        set(ANDROID_EXTRA_LIBS
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libcrypto.so
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libssl.so)
#    endif()
#endif()

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

include(qt6-linguist.cmake)

set(HDR_MAIN source/scwx/qt/main/main_window.hpp)
set(SRC_MAIN source/scwx/qt/main/main.cpp
             source/scwx/qt/main/main_window.cpp)
set(UI_MAIN  source/scwx/qt/main/main_window.ui)
set(HDR_MAP source/scwx/qt/map/map_widget.hpp
            source/scwx/qt/map/radar_layer.hpp
            source/scwx/qt/map/radar_range_layer.hpp
            source/scwx/qt/map/triangle_layer.hpp)
set(SRC_MAP source/scwx/qt/map/map_widget.cpp
            source/scwx/qt/map/radar_layer.cpp
            source/scwx/qt/map/radar_range_layer.cpp
            source/scwx/qt/map/triangle_layer.cpp)
set(HDR_UTIL source/scwx/qt/util/shader_program.hpp)
set(SRC_UTIL source/scwx/qt/util/shader_program.cpp)

set(RESOURCE_FILES scwx-qt.qrc)

set(SHADER_FILES gl/radar.frag
                 gl/radar.vert)

set(TS_FILES ts/scwx_en_US.ts)

set(PROJECT_SOURCES ${HDR_MAIN}
                    ${SRC_MAIN}
                    ${UI_MAIN}
                    ${HDR_MAP}
                    ${SRC_MAP}
                    ${HDR_UTIL}
                    ${SRC_UTIL}
                    ${SHADER_FILES}
                    ${RESOURCE_FILES}
                    ${TS_FILES})

source_group("Header Files\\main" FILES ${HDR_MAIN})
source_group("Source Files\\main" FILES ${SRC_MAIN})
source_group("UI Files\\main"     FILES ${UI_MAIN})
source_group("Header Files\\map"  FILES ${HDR_MAP})
source_group("Source Files\\map"  FILES ${SRC_MAP})
source_group("Header Files\\util" FILES ${HDR_UTIL})
source_group("Source Files\\util" FILES ${SRC_UTIL})
source_group("OpenGL Shaders"     FILES ${SHADER_FILES})
source_group("Resources"          FILES ${RESOURCE_FILES})
source_group("I18N Files"         FILES ${TS_FILES})

qt_add_executable(scwx-qt
    ${PROJECT_SOURCES}
)

qt6_create_translation_scwx(QM_FILES ${scwx-qt_SOURCE_DIR} ${TS_FILES})

target_include_directories(scwx-qt PRIVATE ${scwx-qt_SOURCE_DIR}/source
                                           ${MBGL_INCLUDE_DIR})

target_link_libraries(scwx-qt PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
                                      Qt${QT_VERSION_MAJOR}::OpenGLWidgets
                                      Boost::log
                                      qmapboxgl
                                      opengl32
                                      glm::glm
                                      PROJ::proj)
