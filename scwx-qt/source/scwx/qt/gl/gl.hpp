#pragma once

#if defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wdelete-non-abstract-non-virtual-dtor"
#endif

#include <QOpenGLFunctions_3_3_Core>

#if defined(__clang__)
#   pragma clang diagnostic pop
#endif

#define SCWX_GL_CHECK_ERROR()                                                  \
   {                                                                           \
      GLenum err;                                                              \
      while ((err = gl.glGetError()) != GL_NO_ERROR)                           \
      {                                                                        \
         logger_->error("GL Error: {}, {}: {}", err, __FILE__, __LINE__);      \
      }                                                                        \
   }

namespace scwx
{
namespace qt
{
namespace gl
{

using OpenGLFunctions = QOpenGLFunctions_3_3_Core;

}
} // namespace qt
} // namespace scwx
