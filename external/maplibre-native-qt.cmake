cmake_minimum_required(VERSION 3.24)
set(PROJECT_NAME scwx-mln)

set(gtest_disable_pthreads ON)
set(MLN_WITH_QT ON)
set(MLN_QT_WITH_INTERNAL_ICU ON)
set(MLN_QT_WITH_LOCATION OFF)
set(MLN_CORE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/maplibre-native)
add_subdirectory(maplibre-native-qt)

find_package(ZLIB)
target_include_directories(mbgl-core PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(mbgl-core INTERFACE ${ZLIB_LIBRARIES})

if (MSVC)
    # Produce PDB file for debug
    target_compile_options(mbgl-core PRIVATE "$<$<CONFIG:Release>:/Zi>")
    target_compile_options(MLNQtCore PRIVATE "$<$<CONFIG:Release>:/Zi>")
    target_link_options(MLNQtCore PRIVATE "$<$<CONFIG:Release>:/DEBUG>")
    target_link_options(MLNQtCore PRIVATE "$<$<CONFIG:Release>:/OPT:REF>")
    target_link_options(MLNQtCore PRIVATE "$<$<CONFIG:Release>:/OPT:ICF>")

    # Enable multi-processor compilation
    target_compile_options(MLNQtCore PRIVATE "/MP")
    target_compile_options(mbgl-core PRIVATE "/MP")
    target_compile_options(mbgl-vendor-csscolorparser PRIVATE "/MP")
    target_compile_options(mbgl-vendor-nunicode PRIVATE "/MP")
    target_compile_options(mbgl-vendor-parsedate PRIVATE "/MP")

    if (TARGET mbgl-vendor-sqlite)
        target_compile_options(mbgl-vendor-sqlite PRIVATE "/MP")
    endif()
else()
    target_compile_options(mbgl-core PRIVATE "$<$<CONFIG:Release>:-g>")
    target_compile_options(MLNQtCore PRIVATE "$<$<CONFIG:Release>:-g>")
endif()

set(MLN_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/maplibre-native/include
                     ${CMAKE_CURRENT_SOURCE_DIR}/maplibre-native-qt/src/core/include
                     ${CMAKE_CURRENT_BINARY_DIR}/maplibre-native-qt/src/core/include PARENT_SCOPE)

set_target_properties(test_mln_core PROPERTIES EXCLUDE_FROM_ALL True)
set_target_properties(test_mln_widgets PROPERTIES EXCLUDE_FROM_ALL True)
set_target_properties(MLNQtWidgets PROPERTIES EXCLUDE_FROM_ALL True)

set_target_properties(test_mln_core PROPERTIES FOLDER mln/exclude)
set_target_properties(test_mln_widgets PROPERTIES FOLDER mln/exclude)
set_target_properties(MLNQtWidgets PROPERTIES FOLDER mln/exclude)

set_target_properties(MLNQtCore PROPERTIES FOLDER mln)
set_target_properties(mbgl-core PROPERTIES FOLDER mln)
set_target_properties(mbgl-vendor-csscolorparser PROPERTIES FOLDER mln)
set_target_properties(mbgl-vendor-nunicode PROPERTIES FOLDER mln)
set_target_properties(mbgl-vendor-parsedate PROPERTIES FOLDER mln)

if (TARGET mbgl-vendor-sqlite)
    set_target_properties(mbgl-vendor-sqlite PROPERTIES FOLDER mln)
endif()
