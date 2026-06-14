cmake_minimum_required(VERSION 3.24)
set(PROJECT_NAME scwx-mln)

set(gtest_disable_pthreads ON)

set(MLN_QT_WITH_LOCATION OFF CACHE BOOL "" FORCE)
set(MLN_QT_WITH_WIDGETS OFF CACHE BOOL "" FORCE)
set(MLN_QT_WITH_QUICK_PLUGIN OFF CACHE BOOL "" FORCE)
set(MLN_QT_WITH_INTERNAL_ICU ON CACHE BOOL "" FORCE)

set(MLN_WITH_VULKAN ON CACHE BOOL "" FORCE)
set(MLN_WITH_OPENGL OFF CACHE BOOL "" FORCE)

if (APPLE)
    set(MLN_WITH_METAL OFF CACHE BOOL "" FORCE)
endif()

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

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(
        mbgl-core
        PRIVATE
        "-Wno-sfinae-incomplete"
        "-Wno-error=sfinae-incomplete")
    target_compile_options(
        MLNQtCore
        PRIVATE
        "-Wno-sfinae-incomplete"
        "-Wno-error=sfinae-incomplete")
    if (SCWX_RENDER_BACKEND STREQUAL "VULKAN")
        target_compile_options(mbgl-core PRIVATE "-Wno-unused-parameter")
    endif()
endif()

set(_MLN_VENDOR_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/maplibre-native-qt/vendor/maplibre-native/vendor")
target_include_directories(
    MLNQtCore
    BEFORE
    PRIVATE
    "${_MLN_VENDOR_DIR}/Vulkan-Headers/include"
    "${_MLN_VENDOR_DIR}/VulkanMemoryAllocator/include")

set(MLN_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/maplibre-native-qt/vendor/maplibre-native/include
    ${CMAKE_CURRENT_SOURCE_DIR}/maplibre-native-qt/vendor/maplibre-native/vendor/earcut.hpp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/maplibre-native-qt/src/core/include
    ${CMAKE_CURRENT_BINARY_DIR}/maplibre-native-qt/src/core/include
    PARENT_SCOPE)

if (TARGET test_mln_core)
    set_target_properties(test_mln_core PROPERTIES EXCLUDE_FROM_ALL True)
    set_target_properties(test_mln_core PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD True)
    set_target_properties(test_mln_core PROPERTIES FOLDER mln/exclude)
endif()

if (TARGET test_mln_widgets)
    set_target_properties(test_mln_widgets PROPERTIES EXCLUDE_FROM_ALL True)
    set_target_properties(test_mln_widgets PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD True)
    set_target_properties(test_mln_widgets PROPERTIES FOLDER mln/exclude)
endif()

if (TARGET MLNQtWidgets)
    set_target_properties(MLNQtWidgets PROPERTIES EXCLUDE_FROM_ALL True)
    set_target_properties(MLNQtWidgets PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD True)
    set_target_properties(MLNQtWidgets PROPERTIES FOLDER mln/exclude)
endif()

set_target_properties(MLNQtCore PROPERTIES FOLDER mln)
set_target_properties(mbgl-core PROPERTIES FOLDER mln)
set_target_properties(mbgl-vendor-csscolorparser PROPERTIES FOLDER mln)
set_target_properties(mbgl-vendor-nunicode PROPERTIES FOLDER mln)
set_target_properties(mbgl-vendor-parsedate PROPERTIES FOLDER mln)

if (TARGET mbgl-vendor-sqlite)
    set_target_properties(mbgl-vendor-sqlite PROPERTIES FOLDER mln)
endif()

if (TARGET MLNQtCore_automoc_json_extraction)
    set_target_properties(MLNQtCore_automoc_json_extraction PROPERTIES FOLDER mln)
endif()

if (TARGET MLNQtWidgets_automoc_json_extraction)
    set_target_properties(MLNQtWidgets_automoc_json_extraction PROPERTIES EXCLUDE_FROM_ALL True)
    set_target_properties(MLNQtWidgets_automoc_json_extraction PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD True)
    set_target_properties(MLNQtWidgets_automoc_json_extraction PROPERTIES FOLDER mln/exclude)
endif()
