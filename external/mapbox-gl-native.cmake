cmake_minimum_required(VERSION 3.11)
set(PROJECT_NAME scwx-mbgl)

set(gtest_disable_pthreads ON)
set(MBGL_WITH_QT ON)
add_subdirectory(mapbox-gl-native)

find_package(ZLIB)
target_include_directories(mbgl-core PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(mbgl-core INTERFACE ${ZLIB_LIBRARIES})

set(MBGL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mapbox-gl-native/include PARENT_SCOPE)

set_target_properties(mbgl-qt PROPERTIES EXCLUDE_FROM_ALL True)
set_target_properties(mbgl-test-runner PROPERTIES EXCLUDE_FROM_ALL True)
set_target_properties(mbgl-vendor-icu PROPERTIES EXCLUDE_FROM_ALL True)

set_target_properties(mbgl-benchmark PROPERTIES FOLDER mbgl/exclude)
set_target_properties(mbgl-render-test PROPERTIES FOLDER mbgl/exclude)
set_target_properties(mbgl-test PROPERTIES FOLDER mbgl/exclude)
set_target_properties(mbgl-vendor-benchmark PROPERTIES FOLDER mbgl/exclude)
set_target_properties(mbgl-vendor-googletest PROPERTIES FOLDER mbgl/exclude)
set_target_properties(mbgl-vendor-icu PROPERTIES FOLDER mbgl/exclude)
set_target_properties(mbgl-core-license PROPERTIES FOLDER mbgl/exclude)
set_target_properties(mbgl-qt PROPERTIES FOLDER mbgl/exclude)
set_target_properties(mbgl-qt-docs PROPERTIES FOLDER mbgl/exclude)
set_target_properties(mbgl-test-runner PROPERTIES FOLDER mbgl/exclude)

set_target_properties(mbgl-core PROPERTIES FOLDER mbgl)
set_target_properties(mbgl-vendor-csscolorparser PROPERTIES FOLDER mbgl)
set_target_properties(mbgl-vendor-nunicode PROPERTIES FOLDER mbgl)
set_target_properties(mbgl-vendor-parsedate PROPERTIES FOLDER mbgl)
set_target_properties(mbgl-vendor-sqlite PROPERTIES FOLDER mbgl)
set_target_properties(qmapboxgl PROPERTIES FOLDER mbgl)
