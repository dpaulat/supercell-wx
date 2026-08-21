macro(scwx_output_dirs_setup)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR}/Release/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_BINARY_DIR}/RelWithDebInfo/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_CURRENT_BINARY_DIR}/MinSizeRel/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR}/Debug/bin)

    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR}/Release/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_BINARY_DIR}/RelWithDebInfo/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_CURRENT_BINARY_DIR}/MinSizeRel/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR}/Debug/lib)

    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR}/Release/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_BINARY_DIR}/RelWithDebInfo/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_CURRENT_BINARY_DIR}/MinSizeRel/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR}/Debug/lib)
endmacro()

macro(scwx_python_setup)
    set(SCWX_VIRTUAL_ENV "" CACHE STRING "Python Virtual Environment")

    # Use a Python Virtual Environment
    if (SCWX_VIRTUAL_ENV)
        set(ENV{VIRTUAL_ENV} "${SCWX_VIRTUAL_ENV}")

        if (WIN32)
            set(Python3_EXECUTABLE "$ENV{VIRTUAL_ENV}/Scripts/python.exe")
        else()
            set(Python3_EXECUTABLE "$ENV{VIRTUAL_ENV}/bin/python")
        endif()

        # Add virtual environment to program search paths
        set(CMAKE_PROGRAM_PATH "$ENV{VIRTUAL_ENV}/bin" ${CMAKE_PROGRAM_PATH})

        message(STATUS "Using virtual environment: $ENV{VIRTUAL_ENV}")
    else()
        message(STATUS "Python virtual environment undefined")
    endif()

    # Find Python
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    # Verify we're using the right Python
    message(STATUS "Python executable: ${Python3_EXECUTABLE}")
    message(STATUS "Python version: ${Python3_VERSION}")

    # Only if we are in an application defined virtual environment
    if (SCWX_VIRTUAL_ENV)
        # Setup pip
        set(PIP_ARGS install --upgrade -r "${CMAKE_SOURCE_DIR}/requirements.txt")

        # Install requirements
        execute_process(COMMAND ${Python3_EXECUTABLE} -m pip ${PIP_ARGS}
                        RESULT_VARIABLE PIP_RESULT)
    endif()
endmacro()

# Host windeployqt must query the *target* Qt prefix when QT_HOST_PATH is set
# (Windows ARM64 cross-builds). Empty on native/non-Windows.
function(scwx_windeployqt_qtpaths_options out_var)
    set(_opts)
    if (WIN32 AND DEFINED QT_HOST_PATH AND DEFINED Qt6_DIR)
        get_filename_component(_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
        set(_qtpaths "${_prefix}/bin/qtpaths.bat")
        if (NOT EXISTS "${_qtpaths}")
            message(FATAL_ERROR "Qt paths file not found: ${_qtpaths}")
        endif()
        list(APPEND _opts --qtpaths "${_qtpaths}")
    endif()
    set(${out_var} ${_opts} PARENT_SCOPE)
endfunction()
