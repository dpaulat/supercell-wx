#pragma once

#include <QOpenGLFunctions_3_3_Core>

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
