cmake_minimum_required(VERSION 3.24)
set(PROJECT_NAME scwx-hsluv-c)

set(HSLUV_C_TESTS OFF)
add_subdirectory(hsluv-c)

set(HSLUV_C_INCLUDE_DIR ${hsluv-c_SOURCE_DIR}/src PARENT_SCOPE)

set_target_properties(hsluv-c PROPERTIES FOLDER hsluv)
