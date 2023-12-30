cmake_minimum_required(VERSION 3.20)
set(PROJECT_NAME scwx-mbgl)

set(gtest_disable_pthreads ON)
set(MLN_WITH_QT ON)
set(MLN_QT_WITH_INTERNAL_ICU ON)
add_subdirectory(mapbox-gl-native)

find_package(ZLIB)
target_include_directories(mbgl-core PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(mbgl-core INTERFACE ${ZLIB_LIBRARIES})

if (MSVC)
    # Produce PDB file for debug
    target_compile_options(mbgl-core PRIVATE "$<$<CONFIG:Release>:/Zi>")
    target_compile_options(qmaplibregl PRIVATE "$<$<CONFIG:Release>:/Zi>")
    target_link_options(qmaplibregl PRIVATE "$<$<CONFIG:Release>:/DEBUG>")
    target_link_options(qmaplibregl PRIVATE "$<$<CONFIG:Release>:/OPT:REF>")
    target_link_options(qmaplibregl PRIVATE "$<$<CONFIG:Release>:/OPT:ICF>")
else()
    target_compile_options(mbgl-core PRIVATE "$<$<CONFIG:Release>:-g>")
    target_compile_options(qmaplibregl PRIVATE "$<$<CONFIG:Release>:-g>")
endif()

set(MBGL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mapbox-gl-native/include
                     ${CMAKE_CURRENT_SOURCE_DIR}/mapbox-gl-native/platform/qt/include PARENT_SCOPE)

set_target_properties(mbgl-qt PROPERTIES EXCLUDE_FROM_ALL True)
set_target_properties(mbgl-test-runner PROPERTIES EXCLUDE_FROM_ALL True)

set_target_properties(mbgl-benchmark PROPERTIES FOLDER mbgl/exclude)
set_target_properties(mbgl-render-test PROPERTIES FOLDER mbgl/exclude)
set_target_properties(mbgl-test PROPERTIES FOLDER mbgl/exclude)
set_target_properties(mbgl-vendor-benchmark PROPERTIES FOLDER mbgl/exclude)
set_target_properties(mbgl-vendor-googletest PROPERTIES FOLDER mbgl/exclude)
set_target_properties(mbgl-core-license PROPERTIES FOLDER mbgl/exclude)
set_target_properties(mbgl-qt PROPERTIES FOLDER mbgl/exclude)
set_target_properties(mbgl-test-runner PROPERTIES FOLDER mbgl/exclude)

if (TARGET mbgl-qt-docs)
    set_target_properties(mbgl-qt-docs PROPERTIES FOLDER mbgl/exclude)
endif()

set_target_properties(mbgl-core PROPERTIES FOLDER mbgl)
set_target_properties(mbgl-vendor-csscolorparser PROPERTIES FOLDER mbgl)
set_target_properties(mbgl-vendor-nunicode PROPERTIES FOLDER mbgl)
set_target_properties(mbgl-vendor-parsedate PROPERTIES FOLDER mbgl)
set_target_properties(qmaplibregl PROPERTIES FOLDER mbgl)
