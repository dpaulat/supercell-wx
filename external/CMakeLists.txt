cmake_minimum_required(VERSION 3.20)
set(PROJECT_NAME scwx-external)

set_property(DIRECTORY
             APPEND
             PROPERTY CMAKE_CONFIGURE_DEPENDS
             aws-sdk-cpp.cmake
             freetype-gl.cmake
             hsluv-c.cmake
             imgui.cmake
             mapbox-gl-native.cmake
             stb.cmake)

include(aws-sdk-cpp.cmake)
include(freetype-gl.cmake)
include(hsluv-c.cmake)
include(imgui.cmake)
include(mapbox-gl-native.cmake)
include(stb.cmake)
