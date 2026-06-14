cmake_minimum_required(VERSION 3.24)
set(PROJECT_NAME scwx-external)

set_property(DIRECTORY
             APPEND
             PROPERTY CMAKE_CONFIGURE_DEPENDS
             aws-sdk-cpp.cmake
             date.cmake
             hsluv-c.cmake
             imgui.cmake
             maplibre-native-qt.cmake
             stb.cmake
             textflowcpp.cmake
             units.cmake
             qt6ct.cmake)

include(aws-sdk-cpp.cmake)
include(date.cmake)
include(hsluv-c.cmake)
include(imgui.cmake)
include(maplibre-native-qt.cmake)
include(stb.cmake)
include(textflowcpp.cmake)
include(units.cmake)
include(qt6ct.cmake)
