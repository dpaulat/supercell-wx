cmake_minimum_required(VERSION 3.20)
set(PROJECT_NAME scwx-external)

set_property(DIRECTORY
             APPEND
             PROPERTY CMAKE_CONFIGURE_DEPENDS
             aws-sdk-cpp.cmake
             date.cmake
             freetype-gl.cmake
             hsluv-c.cmake
             imgui.cmake
             mapbox-gl-native.cmake
             stb.cmake
             textflowcpp.cmake
             units.cmake)

include(aws-sdk-cpp.cmake)
include(date.cmake)
include(freetype-gl.cmake)
include(hsluv-c.cmake)
include(imgui.cmake)
include(mapbox-gl-native.cmake)
include(stb.cmake)
include(textflowcpp.cmake)
include(units.cmake)
