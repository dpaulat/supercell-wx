#include <scwx/qt/gl/shader_program.hpp>
#include <scwx/util/logger.hpp>

#include <QFile>

namespace scwx
{
namespace qt
{
namespace gl
{

static const std::string logPrefix_ = "scwx::qt::gl::shader_program";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr GLsizei kInfoLogBufSize = 512;

static const std::unordered_map<GLenum, std::string> kShaderNames_ {
   {GL_VERTEX_SHADER, "vertex"},
   {GL_GEOMETRY_SHADER, "geometry"},
   {GL_FRAGMENT_SHADER, "fragment"}};

class ShaderProgram::Impl
{
public:
   explicit Impl(OpenGLFunctions& gl) : gl_(gl), id_ {GL_INVALID_INDEX}
   {
      // Create shader program
      id_ = gl_.glCreateProgram();
   }

   ~Impl()
   {
      // Delete shader program
      gl_.glDeleteProgram(id_);
   }

   static std::string ShaderName(GLenum type);

   OpenGLFunctions& gl_;

   GLuint id_;
};

ShaderProgram::ShaderProgram(OpenGLFunctions& gl) :
    p(std::make_unique<Impl>(gl))
{
}
ShaderProgram::~ShaderProgram() = default;

ShaderProgram::ShaderProgram(ShaderProgram&&) noexcept            = default;
ShaderProgram& ShaderProgram::operator=(ShaderProgram&&) noexcept = default;

GLuint ShaderProgram::id() const
{
   return p->id_;
}

std::string ShaderProgram::Impl::ShaderName(GLenum type)
{
   auto it = kShaderNames_.find(type);
   if (it != kShaderNames_.cend())
   {
      return it->second;
   }
   return fmt::format("{:#06x}", type);
}

bool ShaderProgram::Load(const std::string& vertexPath,
                         const std::string& fragmentPath)
{
   return Load({{GL_VERTEX_SHADER, vertexPath}, //
                {GL_FRAGMENT_SHADER, fragmentPath}});
}

bool ShaderProgram::Load(
   std::initializer_list<std::pair<GLenum, std::string>> shaders)
{
   logger_->debug("Load()");

   OpenGLFunctions& gl = p->gl_;

   GLint   glSuccess;
   bool    success = true;
   char    infoLog[kInfoLogBufSize];
   GLsizei logLength;

   std::vector<GLuint> shaderIds {};

   for (auto& shader : shaders)
   {
      logger_->debug("Loading {} shader: {}",
                     Impl::ShaderName(shader.first),
                     shader.second);

      QFile file(shader.second.c_str());
      file.open(QIODevice::ReadOnly | QIODevice::Text);

      if (!file.isOpen())
      {
         logger_->error("Could not load shader");
         success = false;
         break;
      }

      QTextStream shaderStream(&file);
      shaderStream.setEncoding(QStringConverter::Utf8);

      std::string shaderSource  = shaderStream.readAll().toStdString();
      const char* shaderSourceC = shaderSource.c_str();

      // Create a vertex shader
      GLuint shaderId = gl.glCreateShader(shader.first);
      shaderIds.push_back(shaderId);

      // Attach the shader source code and compile the shader
      gl.glShaderSource(shaderId, 1, &shaderSourceC, NULL);
      gl.glCompileShader(shaderId);

      // Check for errors
      gl.glGetShaderiv(shaderId, GL_COMPILE_STATUS, &glSuccess);
      gl.glGetShaderInfoLog(shaderId, kInfoLogBufSize, &logLength, infoLog);
      if (!glSuccess)
      {
         logger_->error("Shader compilation failed: {}", infoLog);
         success = false;
         break;
      }
      else if (logLength > 0)
      {
         logger_->error("Shader compiled with warnings: {}", infoLog);
      }
   }

   if (success)
   {
      for (auto& shaderId : shaderIds)
      {
         gl.glAttachShader(p->id_, shaderId);
      }
      gl.glLinkProgram(p->id_);

      // Check for errors
      gl.glGetProgramiv(p->id_, GL_LINK_STATUS, &glSuccess);
      gl.glGetProgramInfoLog(p->id_, kInfoLogBufSize, &logLength, infoLog);
      if (!glSuccess)
      {
         logger_->error("Shader program link failed: {}", infoLog);
         success = false;
      }
      else if (logLength > 0)
      {
         logger_->error("Shader program linked with warnings: {}", infoLog);
      }
   }

   // Delete shaders
   for (auto& shaderId : shaderIds)
   {
      gl.glDeleteShader(shaderId);
   }

   return success;
}

void ShaderProgram::Use() const
{
   p->gl_.glUseProgram(p->id_);
}

} // namespace gl
} // namespace qt
} // namespace scwx
