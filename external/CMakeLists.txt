cmake_minimum_required(VERSION 3.11)
set(PROJECT_NAME scwx-external)

set_property(DIRECTORY
             APPEND
             PROPERTY CMAKE_CONFIGURE_DEPENDS
             freetype-gl.cmake
             hsluv-c.cmake
             mapbox-gl-native.cmake)

include(freetype-gl.cmake)
include(hsluv-c.cmake)
include(mapbox-gl-native.cmake)
