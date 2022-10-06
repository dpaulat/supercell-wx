cmake_minimum_required(VERSION 3.20)

set_property(DIRECTORY
             APPEND
             PROPERTY CMAKE_CONFIGURE_DEPENDS
             wxdata.cmake)

include(wxdata.cmake)
