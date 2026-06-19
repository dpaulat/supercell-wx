#include <scwx/qt/gl/shader_program.hpp>
#include <scwx/util/logger.hpp>

#include <QFile>
#include <QTextStream>

namespace scwx::qt::gl
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
   explicit Impl() : id_ {glCreateProgram()} {}

   ~Impl()
   {
      // Delete shader program
      glDeleteProgram(id_);
   }

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   static std::string ShaderName(GLenum type);

   GLuint id_;
};

ShaderProgram::ShaderProgram() : p(std::make_unique<Impl>()) {}
ShaderProgram::~ShaderProgram() = default;

ShaderProgram::ShaderProgram(ShaderProgram&&) noexcept            = default;
ShaderProgram& ShaderProgram::operator=(ShaderProgram&&) noexcept = default;

GLuint ShaderProgram::id() const
{
   return p->id_;
}

GLint ShaderProgram::GetUniformLocation(const std::string& name)
{
   const GLint location = glGetUniformLocation(p->id_, name.c_str());
   if (location == -1)
   {
      logger_->warn("Could not find {}", name);
   }
   return location;
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

      const bool isOpen = file.open(QIODevice::ReadOnly | QIODevice::Text);

      if (!isOpen || !file.isOpen())
      {
         logger_->error("Could not load shader");
         success = false;
         break;
      }

      QTextStream shaderStream(&file);
      shaderStream.setEncoding(QStringConverter::Utf8);

      std::string shaderSource  = shaderStream.readAll().toStdString();
      const char* shaderSourceC = shaderSource.c_str();

      // Create a shader
      const GLuint shaderId = glCreateShader(shader.first);
      shaderIds.push_back(shaderId);

      // Attach the shader source code and compile the shader
      glShaderSource(shaderId, 1, &shaderSourceC, nullptr);
      glCompileShader(shaderId);

      // Check for errors
      glGetShaderiv(shaderId, GL_COMPILE_STATUS, &glSuccess);
      glGetShaderInfoLog(
         shaderId, kInfoLogBufSize, &logLength, static_cast<GLchar*>(infoLog));
      if (!glSuccess)
      {
         logger_->error("Shader compilation failed: {}", infoLog);
         success = false;
         break;
      }
      else if (logLength > 0)
      {
         logger_->warn("Shader compiled with warnings: {}", infoLog);
      }
   }

   if (success)
   {
      for (auto& shaderId : shaderIds)
      {
         glAttachShader(p->id_, shaderId);
      }
      glLinkProgram(p->id_);

      // Check for errors
      glGetProgramiv(p->id_, GL_LINK_STATUS, &glSuccess);
      glGetProgramInfoLog(
         p->id_, kInfoLogBufSize, &logLength, static_cast<GLchar*>(infoLog));
      if (!glSuccess)
      {
         logger_->error("Shader program link failed: {}", infoLog);
         success = false;
      }
      else if (logLength > 0)
      {
         logger_->warn("Shader program linked with warnings: {}", infoLog);
      }
   }

   // Delete shaders
   for (auto& shaderId : shaderIds)
   {
      glDeleteShader(shaderId);
   }

   return success;
}

void ShaderProgram::Use() const
{
   glUseProgram(p->id_);
}

} // namespace scwx::qt::gl
