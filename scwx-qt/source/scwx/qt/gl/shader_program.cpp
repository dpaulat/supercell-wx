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

static constexpr GLsizei INFO_LOG_BUF_SIZE = 512;

class ShaderProgramImpl
{
public:
   explicit ShaderProgramImpl(OpenGLFunctions& gl) :
       gl_(gl), id_ {GL_INVALID_INDEX}
   {
      // Create shader program
      id_ = gl_.glCreateProgram();
   }

   ~ShaderProgramImpl()
   {
      // Delete shader program
      gl_.glDeleteProgram(id_);
   }

   OpenGLFunctions& gl_;

   GLuint id_;
};

ShaderProgram::ShaderProgram(OpenGLFunctions& gl) :
    p(std::make_unique<ShaderProgramImpl>(gl))
{
}
ShaderProgram::~ShaderProgram() = default;

ShaderProgram::ShaderProgram(ShaderProgram&&) noexcept = default;
ShaderProgram& ShaderProgram::operator=(ShaderProgram&&) noexcept = default;

GLuint ShaderProgram::id() const
{
   return p->id_;
}

bool ShaderProgram::Load(const std::string& vertexPath,
                         const std::string& fragmentPath)
{
   logger_->debug("Load: {}, {}", vertexPath, fragmentPath);

   OpenGLFunctions& gl = p->gl_;

   GLint   glSuccess;
   bool    success = true;
   char    infoLog[INFO_LOG_BUF_SIZE];
   GLsizei logLength;

   QFile vertexFile(vertexPath.c_str());
   QFile fragmentFile(fragmentPath.c_str());

   vertexFile.open(QIODevice::ReadOnly | QIODevice::Text);
   fragmentFile.open(QIODevice::ReadOnly | QIODevice::Text);

   if (!vertexFile.isOpen())
   {
      logger_->error("Could not load vertex shader: {}", vertexPath);
      return false;
   }

   if (!fragmentFile.isOpen())
   {
      logger_->error("Could not load fragment shader: {}", fragmentPath);
      return false;
   }

   QTextStream vertexShaderStream(&vertexFile);
   QTextStream fragmentShaderStream(&fragmentFile);

   vertexShaderStream.setEncoding(QStringConverter::Utf8);
   fragmentShaderStream.setEncoding(QStringConverter::Utf8);

   std::string vertexShaderSource = vertexShaderStream.readAll().toStdString();
   std::string fragmentShaderSource =
      fragmentShaderStream.readAll().toStdString();

   const char* vertexShaderSourceC   = vertexShaderSource.c_str();
   const char* fragmentShaderSourceC = fragmentShaderSource.c_str();

   // Create a vertex shader
   GLuint vertexShader = gl.glCreateShader(GL_VERTEX_SHADER);

   // Attach the shader source code and compile the shader
   gl.glShaderSource(vertexShader, 1, &vertexShaderSourceC, NULL);
   gl.glCompileShader(vertexShader);

   // Check for errors
   gl.glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &glSuccess);
   gl.glGetShaderInfoLog(vertexShader, INFO_LOG_BUF_SIZE, &logLength, infoLog);
   if (!glSuccess)
   {
      logger_->error("Vertex shader compilation failed: {}", infoLog);
      success = false;
   }
   else if (logLength > 0)
   {
      logger_->error("Vertex shader compiled with warnings: {}", infoLog);
   }

   // Create a fragment shader
   GLuint fragmentShader = gl.glCreateShader(GL_FRAGMENT_SHADER);

   // Attach the shader source and compile the shader
   gl.glShaderSource(fragmentShader, 1, &fragmentShaderSourceC, NULL);
   gl.glCompileShader(fragmentShader);

   // Check for errors
   gl.glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &glSuccess);
   gl.glGetShaderInfoLog(
      fragmentShader, INFO_LOG_BUF_SIZE, &logLength, infoLog);
   if (!glSuccess)
   {
      logger_->error("Fragment shader compilation failed: {}", infoLog);
      success = false;
   }
   else if (logLength > 0)
   {
      logger_->error("Fragment shader compiled with warnings: {}", infoLog);
   }

   if (success)
   {
      gl.glAttachShader(p->id_, vertexShader);
      gl.glAttachShader(p->id_, fragmentShader);
      gl.glLinkProgram(p->id_);

      // Check for errors
      gl.glGetProgramiv(p->id_, GL_LINK_STATUS, &glSuccess);
      gl.glGetProgramInfoLog(p->id_, INFO_LOG_BUF_SIZE, &logLength, infoLog);
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
   gl.glDeleteShader(vertexShader);
   gl.glDeleteShader(fragmentShader);

   return success;
}

void ShaderProgram::Use() const
{
   p->gl_.glUseProgram(p->id_);
}

} // namespace gl
} // namespace qt
} // namespace scwx
