cmake_minimum_required(VERSION 3.24)

set_property(DIRECTORY
             APPEND
             PROPERTY CMAKE_CONFIGURE_DEPENDS
             scwx-qt.cmake)

include(scwx-qt.cmake)

# Find the required Qt components
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Add the executable or library for this subdirectory
add_library(scwx_qt_lib STATIC
    source/scwx/qt/main/main_window.cpp
    source/scwx/qt/main/main_window.hpp
    # Add other source files here
)

set(SCWX_QT_UI_SRC
    source/scwx/qt/ui/radar_product_selector_widget.cpp
    # Add other source files here
)

set(SCWX_QT_UI_HDR
    source/scwx/qt/ui/radar_product_selector_widget.hpp
    # Add other header files here
)

# Add the UI source and header files to the target
target_sources(scwx_qt_lib PRIVATE
    ${SCWX_QT_UI_SRC}
    ${SCWX_QT_UI_HDR}
)

# Add include directories
target_include_directories(scwx_qt_lib PRIVATE
    source/scwx/qt/main
    source/scwx/qt/ui
)

# Link the Qt libraries to the target
target_link_libraries(scwx_qt_lib PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
