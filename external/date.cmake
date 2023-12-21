cmake_minimum_required(VERSION 3.24)
set(PROJECT_NAME scwx-date)

set(USE_SYSTEM_TZ_DB ON)
set(BUILD_TZ_LIB     ON)

add_subdirectory(date)
