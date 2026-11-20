cmake_minimum_required(VERSION 3.24)
set(PROJECT_NAME scwx-mln)

set(gtest_disable_pthreads ON)
set(MLN_WITH_QT ON)
set(MLN_QT_WITH_INTERNAL_ICU ON)
set(MLN_QT_WITH_LOCATION OFF)
set(MLN_CORE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/maplibre-native)
add_subdirectory(maplibre-native-qt)

# MSVC 14.52.37025 ICEs in the front-end (msc1.cpp) on inherited
# mapbox::variant constructors of Result (C1001 at literal.hpp while
# compiling annotation_manager.cpp and other mbgl-core TUs). 14.51.36260
# compiles the same sources. maplibre-native#5004 works around it by
# dropping `using variant<...>::variant;`; Result(U&&) still provides
# the conversions. Overlay the patched header in the build tree so the
# MapLibre submodules stay unmodified. mbgl-core compiles the sibling
# tree from MLN_CORE_PATH.
set(_mln_core_src "${CMAKE_CURRENT_SOURCE_DIR}/maplibre-native")
if(NOT EXISTS "${_mln_core_src}/include/mbgl/style/expression/expression.hpp")
    set(_mln_core_src "$CACHE{MLN_CORE_PATH}")
endif()
set(_mln_expression_hpp "${_mln_core_src}/include/mbgl/style/expression/expression.hpp")
set(_mln_workaround_dir "${CMAKE_CURRENT_BINARY_DIR}/mln-msvc-14.52-workaround")
set(_mln_workaround_hpp "${_mln_workaround_dir}/mbgl/style/expression/expression.hpp")
if(EXISTS "${_mln_expression_hpp}")
    file(READ "${_mln_expression_hpp}" _mln_expression_contents)
    if(_mln_expression_contents MATCHES "using variant<EvaluationError, T>::variant;")
        string(REGEX REPLACE
            "[ \t]*using variant<EvaluationError, T>::variant;"
            "    // Inherited mapbox::variant constructors ICE MSVC 14.52\n    // (https://github.com/maplibre/maplibre-native/issues/5004)."
            _mln_expression_contents "${_mln_expression_contents}")
        message(STATUS "Applying MapLibre MSVC 14.52 C1001 workaround (maplibre-native#5004)")
    endif()
    file(MAKE_DIRECTORY "${_mln_workaround_dir}/mbgl/style/expression")
    file(WRITE "${_mln_workaround_hpp}" "${_mln_expression_contents}")
    target_include_directories(mbgl-core BEFORE PUBLIC "${_mln_workaround_dir}")
    target_include_directories(MLNQtCore BEFORE PRIVATE "${_mln_workaround_dir}")
    unset(_mln_expression_contents)
endif()
unset(_mln_core_src)
unset(_mln_expression_hpp)
unset(_mln_workaround_hpp)

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

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
    CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16)
    target_compile_options(mbgl-core PRIVATE "-Wno-sfinae-incomplete")
endif()

if (APPLE)
    # Enable GL check error debug
    target_compile_definitions(mbgl-core PRIVATE MLN_GL_CHECK_ERRORS=1)
    target_compile_definitions(MLNQtCore PRIVATE MLN_GL_CHECK_ERRORS=1)
endif()

set(MLN_INCLUDE_DIRS ${_mln_workaround_dir}
                     ${CMAKE_CURRENT_SOURCE_DIR}/maplibre-native/include
                     ${CMAKE_CURRENT_SOURCE_DIR}/maplibre-native-qt/src/core/include
                     ${CMAKE_CURRENT_BINARY_DIR}/maplibre-native-qt/src/core/include PARENT_SCOPE)
unset(_mln_workaround_dir)

set_target_properties(test_mln_core PROPERTIES EXCLUDE_FROM_ALL True)
set_target_properties(test_mln_widgets PROPERTIES EXCLUDE_FROM_ALL True)
set_target_properties(MLNQtWidgets PROPERTIES EXCLUDE_FROM_ALL True)

set_target_properties(test_mln_core PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD True)
set_target_properties(test_mln_widgets PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD True)
set_target_properties(MLNQtWidgets PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD True)

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

if (TARGET MLNQtCore_automoc_json_extraction)
    set_target_properties(MLNQtCore_automoc_json_extraction PROPERTIES FOLDER mln)
endif()

if (TARGET MLNQtWidgets_automoc_json_extraction)
    set_target_properties(MLNQtWidgets_automoc_json_extraction PROPERTIES EXCLUDE_FROM_ALL True)
    set_target_properties(MLNQtWidgets_automoc_json_extraction PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD True)
    set_target_properties(MLNQtWidgets_automoc_json_extraction PROPERTIES FOLDER mln/exclude)
endif()
