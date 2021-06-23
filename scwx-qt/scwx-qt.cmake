cmake_minimum_required(VERSION 3.11)

project(scwx-qt LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

set(TS_FILES ts/scwx_en_US.ts)

set(PROJECT_SOURCES
        source/scwx/qt/main.cpp
        source/scwx/qt/main_window.cpp
        source/scwx/qt/main_window.hpp
        source/scwx/qt/main_window.ui
        ${TS_FILES}
)

qt_add_executable(scwx-qt
    ${PROJECT_SOURCES}
)

qt6_create_translation_scwx(QM_FILES ${PROJECT_SOURCE_DIR} ${TS_FILES})

target_link_libraries(scwx-qt PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
