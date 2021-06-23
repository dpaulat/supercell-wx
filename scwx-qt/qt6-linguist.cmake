#=============================================================================
# Copyright (C) 2020 The Qt Company Ltd.
# Copyright 2005-2011 Kitware, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# * Neither the name of Kitware, Inc. nor the names of its
#   contributors may be used to endorse or promote products derived
#   from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#=============================================================================

# Modified to remove include directories from translation

function(qt6_create_translation_scwx _qm_files)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs OPTIONS)

    cmake_parse_arguments(_LUPDATE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(_lupdate_files ${_LUPDATE_UNPARSED_ARGUMENTS})
    set(_lupdate_options ${_LUPDATE_OPTIONS})

    list(FIND _lupdate_options "-extensions" _extensions_index)
    if(_extensions_index GREATER -1)
        math(EXPR _extensions_index "${_extensions_index} + 1")
        list(GET _lupdate_options ${_extensions_index} _extensions_list)
        string(REPLACE "," ";" _extensions_list "${_extensions_list}")
        list(TRANSFORM _extensions_list STRIP)
        list(TRANSFORM _extensions_list REPLACE "^\." "")
        list(TRANSFORM _extensions_list PREPEND "*.")
    else()
        set(_extensions_list "*.java;*.jui;*.ui;*.c;*.c++;*.cc;*.cpp;*.cxx;*.ch;*.h;*.h++;*.hh;*.hpp;*.hxx;*.js;*.qs;*.qml;*.qrc")
    endif()
    set(_my_sources)
    set(_my_tsfiles)
    foreach(_file ${_lupdate_files})
        get_filename_component(_ext ${_file} EXT)
        get_filename_component(_abs_FILE ${_file} ABSOLUTE)
        if(_ext MATCHES "ts")
            list(APPEND _my_tsfiles ${_abs_FILE})
        else()
            list(APPEND _my_sources ${_abs_FILE})
        endif()
    endforeach()
    foreach(_ts_file ${_my_tsfiles})
        if(_my_sources)
          # make a list file to call lupdate on, so we don't make our commands too
          # long for some systems
          get_filename_component(_ts_name ${_ts_file} NAME)
          set(_ts_lst_file "${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/${_ts_name}_lst_file")
          set(_lst_file_srcs)
          set(_dependencies)
          foreach(_lst_file_src ${_my_sources})
              set(_lst_file_srcs "${_lst_file_src}\n${_lst_file_srcs}")
              if(IS_DIRECTORY ${_lst_file_src})
                  list(TRANSFORM _extensions_list PREPEND "${_lst_file_src}/" OUTPUT_VARIABLE _directory_glob)
                  file(GLOB_RECURSE _directory_contents CONFIGURE_DEPENDS ${_directory_glob})
                  list(APPEND _dependencies ${_directory_contents})
              else()
                  list(APPEND _dependencies "${_lst_file_src}")
              endif()
          endforeach()

          # get_directory_property(_inc_DIRS INCLUDE_DIRECTORIES)
          # foreach(_pro_include ${_inc_DIRS})
          #     get_filename_component(_abs_include "${_pro_include}" ABSOLUTE)
          #     set(_lst_file_srcs "-I${_pro_include}\n${_lst_file_srcs}")
          # endforeach()

          file(WRITE ${_ts_lst_file} "${_lst_file_srcs}")
        endif()
        add_custom_command(OUTPUT ${_ts_file}
            COMMAND ${QT_CMAKE_EXPORT_NAMESPACE}::lupdate
            ARGS ${_lupdate_options} "@${_ts_lst_file}" -ts ${_ts_file}
            DEPENDS ${_dependencies}
            VERBATIM)
    endforeach()
    qt6_add_translation(${_qm_files} ${_my_tsfiles})
    set(${_qm_files} ${${_qm_files}} PARENT_SCOPE)
endfunction()