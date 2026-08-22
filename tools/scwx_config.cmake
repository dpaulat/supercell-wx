# Windows ARM64 Qt is a cross-build: libraries live in the target prefix (QT_ROOT_DIR)
# while moc/rcc/qtpaths/windeployqt must come from the x64 host prefix (QT_HOST_PATH).
# Qt 6.10+ ships Qt6*Tools CMake packages in the target prefix that point at
# target-arch binaries (and renamed qtpaths.bat to host-qtpaths.bat). Force host
# tools and Qt's toolchain before project() so find_package(Qt6) does not load
# those target tool packages.
if(WIN32 AND NOT CMAKE_TOOLCHAIN_FILE)
    set(_scwx_qt_host "$ENV{QT_HOST_PATH}")
    set(_scwx_qt_root "$ENV{QT_ROOT_DIR}")
    if(_scwx_qt_host AND _scwx_qt_root)
        set(_scwx_qt_tc "${_scwx_qt_root}/lib/cmake/Qt6/qt.toolchain.cmake")
        if(EXISTS "${_scwx_qt_tc}")
            set(CMAKE_TOOLCHAIN_FILE "${_scwx_qt_tc}" CACHE FILEPATH
                "Qt cross-compile toolchain")
            message(STATUS "Using Qt cross-compile toolchain: ${_scwx_qt_tc}")
            message(STATUS "Using host Qt tools from: ${_scwx_qt_host}")
        endif()
        file(GLOB _scwx_host_tools_dirs "${_scwx_qt_host}/lib/cmake/Qt6*Tools")
        foreach(_scwx_tools_dir IN LISTS _scwx_host_tools_dirs)
            get_filename_component(_scwx_tools_name "${_scwx_tools_dir}" NAME)
            if(_scwx_tools_name MATCHES "UiTools$"
               OR _scwx_tools_name MATCHES "ShaderTools$"
               OR _scwx_tools_name STREQUAL "Qt6Tools")
                continue()
            endif()
            if(NOT DEFINED ${_scwx_tools_name}_DIR)
                set(${_scwx_tools_name}_DIR "${_scwx_tools_dir}" CACHE PATH
                    "Host Qt tools package")
            endif()
        endforeach()
        unset(_scwx_host_tools_dirs)
        unset(_scwx_tools_dir)
        unset(_scwx_tools_name)
        unset(_scwx_qt_tc)
    endif()
    unset(_scwx_qt_host)
    unset(_scwx_qt_root)
endif()

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
# Qt 6.8 used qtpaths.bat; 6.10+ renamed the x64 wrapper to host-qtpaths.bat
# (qtpaths.exe in the same directory is the ARM64 binary and will not run on
# an x64 host). Prefer the host wrapper.
function(scwx_windeployqt_qtpaths_options out_var)
    set(_opts)
    if (WIN32 AND DEFINED QT_HOST_PATH AND DEFINED Qt6_DIR)
        get_filename_component(_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
        set(_qtpaths "")
        foreach(_candidate IN ITEMS
                host-qtpaths.bat host-qtpaths6.bat qtpaths.bat qtpaths6.bat)
            if (EXISTS "${_prefix}/bin/${_candidate}")
                set(_qtpaths "${_prefix}/bin/${_candidate}")
                break()
            endif()
        endforeach()
        if (NOT _qtpaths)
            message(FATAL_ERROR
                "Qt paths helper not found under ${_prefix}/bin "
                "(tried host-qtpaths.bat, qtpaths.bat)")
        endif()
        message(STATUS "windeployqt --qtpaths ${_qtpaths}")
        list(APPEND _opts --qtpaths "${_qtpaths}")
    endif()
    set(${out_var} ${_opts} PARENT_SCOPE)
endfunction()
