cmake_minimum_required(VERSION 3.11)
set(PROJECT_NAME scwx-freetype-gl)

find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED)
find_package(GLEW REQUIRED)

set(freetype-gl_WITH_GLEW      ON)
set(freetype-gl_WITH_GLAD      OFF)
set(freetype-gl_USE_VAO        ON)
set(freetype-gl_BUILD_DEMOS    OFF)
set(freetype-gl_BUILD_APIDOC   ON)
set(freetype-gl_BUILD_HARFBUZZ OFF)
set(freetype-gl_BUILD_MAKEFONT ON)
set(freetype-gl_BUILD_TESTS    OFF)
set(freetype-gl_BUILD_SHARED   OFF)
set(freetype-gl_OFF_SCREEN     OFF)

configure_file(freetype-gl/cmake/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/freetype-gl/config.h)

set(FREETYPE_GL_HDR freetype-gl/distance-field.h
                    freetype-gl/edtaa3func.h
                    freetype-gl/font-manager.h
                    freetype-gl/freetype-gl.h
                    freetype-gl/markup.h
                    freetype-gl/opengl.h
                    freetype-gl/platform.h
                    freetype-gl/text-buffer.h
                    freetype-gl/texture-atlas.h
                    freetype-gl/texture-font.h
                    freetype-gl/utf8-utils.h
                    freetype-gl/ftgl-utils.h
                    freetype-gl/vec234.h
                    freetype-gl/vector.h
                    freetype-gl/vertex-attribute.h
                    freetype-gl/vertex-buffer.h
                    freetype-gl/freetype-gl-errdef.h
                    ${CMAKE_CURRENT_BINARY_DIR}/freetype-gl/config.h)
set(FREETYPE_GL_SRC freetype-gl/distance-field.c
                    freetype-gl/edtaa3func.c
                    freetype-gl/font-manager.c
                    freetype-gl/platform.c
                    freetype-gl/text-buffer.c
                    freetype-gl/texture-atlas.c
                    freetype-gl/texture-font.c
                    freetype-gl/utf8-utils.c
                    freetype-gl/ftgl-utils.c
                    freetype-gl/vector.c
                    freetype-gl/vertex-attribute.c
                    freetype-gl/vertex-buffer.c)
                    
include(CheckLibraryExists)
check_library_exists(m cos "" HAVE_MATH_LIBRARY)

if(HAVE_MATH_LIBRARY)
    list(APPEND CMAKE_REQUIRED_LIBRARIES m)
    set(MATH_LIBRARY m)
endif()

if(freetype-gl_BUILD_APIDOC)
    add_subdirectory(freetype-gl/doc)
endif()

if(freetype-gl_BUILD_SHARED)
    add_library(freetype-gl SHARED ${FREETYPE_GL_SRC}
                                   ${FREETYPE_GL_HDR})
    set_target_properties(freetype-gl PROPERTIES VERSION 0.3.2 SOVERSION 0)
    target_link_libraries(freetype-gl PRIVATE opengl::opengl
                                              Freetype::Freetype
                                              ${MATH_LIBRARY}
                                              GLEW::GLEW)
else()
    add_library(freetype-gl STATIC ${FREETYPE_GL_SRC}
                                   ${FREETYPE_GL_HDR})
    target_link_libraries(freetype-gl PUBLIC opengl::opengl
                                             Freetype::Freetype
                                             ${MATH_LIBRARY}
                                             GLEW::GLEW)
endif()

if(freetype-gl_BUILD_MAKEFONT)
    add_executable(makefont freetype-gl/makefont.c)
    target_link_libraries(makefont freetype-gl
                                   GLEW::GLEW)
endif()

if(freetype-gl_USE_VAO)
    target_compile_definitions(freetype-gl PRIVATE FREETYPE_GL_USE_VAO)
    target_compile_definitions(makefont    PRIVATE FREETYPE_GL_USE_VAO)
endif()

if(freetype-gl_USE_WITH_GLAD)
    target_compile_definitions(freetype-gl PRIVATE GL_WITH_GLAD)
    target_compile_definitions(makefont    PRIVATE GL_WITH_GLAD)
endif()

if(freetype-gl_USE_WITH_GLEW)
    target_compile_definitions(freetype-gl PRIVATE FREETYPE_GL_USE_GLEW)
    target_compile_definitions(makefont    PRIVATE FREETYPE_GL_USE_GLEW)
endif()

if(MSVC)
    target_compile_definitions(freetype-gl PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    target_compile_definitions(makefont    PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
endif(MSVC)

target_include_directories(freetype-gl PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/freetype-gl)
target_include_directories(makefont    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/freetype-gl)

set(FTGL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/freetype-gl PARENT_SCOPE)

set_target_properties(doc      PROPERTIES EXCLUDE_FROM_ALL True)
set_target_properties(makefont PROPERTIES EXCLUDE_FROM_ALL True)

set_target_properties(freetype-gl PROPERTIES FOLDER ftgl)
set_target_properties(makefont    PROPERTIES FOLDER ftgl)
set_target_properties(doc         PROPERTIES FOLDER ftgl)
