#include <scwx/qt/map/triangle_layer.hpp>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ = "[scwx::qt::map::triangle_layer] ";

class TriangleLayerImpl
{
public:
   explicit TriangleLayerImpl(gl::OpenGLFunctions& gl) :
       gl_(gl),
       shaderProgram_ {GL_INVALID_INDEX},
       vbo_ {GL_INVALID_INDEX},
       vao_ {GL_INVALID_INDEX}
   {
      gl_.initializeOpenGLFunctions();
   }
   ~TriangleLayerImpl() = default;

   gl::OpenGLFunctions& gl_;

   GLuint shaderProgram_;
   GLuint vbo_;
   GLuint vao_;
};

TriangleLayer::TriangleLayer(gl::OpenGLFunctions& gl) :
    p(std::make_unique<TriangleLayerImpl>(gl))
{
}
TriangleLayer::~TriangleLayer() = default;

TriangleLayer::TriangleLayer(TriangleLayer&&) noexcept = default;
TriangleLayer& TriangleLayer::operator=(TriangleLayer&&) noexcept = default;

void TriangleLayer::initialize()
{
   gl::OpenGLFunctions& gl = p->gl_;

   static const char* vertexShaderSource =
      "#version 330 core\n"
      "layout (location = 0) in vec3 aPos;"
      "void main()"
      "{"
      "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);"
      "}";

   static const char* fragmentShaderSource =
      "#version 330 core\n"
      "out vec4 FragColor;"
      "void main()"
      "{"
      "   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);"
      "}";

   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "initialize()";

   GLint success;

   // Create a vertex shader
   GLuint vertexShader = gl.glCreateShader(GL_VERTEX_SHADER);

   // Attach the shader source code and compile the shader
   gl.glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
   gl.glCompileShader(vertexShader);

   // Check for errors
   gl.glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
   if (!success)
   {
      char infoLog[512];
      gl.glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
      BOOST_LOG_TRIVIAL(error)
         << logPrefix_ << "Vertex shader compilation failed";
   }

   // Create a fragment shader
   GLuint fragmentShader = gl.glCreateShader(GL_FRAGMENT_SHADER);

   // Attach the shader source and compile the shader
   gl.glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
   gl.glCompileShader(fragmentShader);

   // Check for errors
   gl.glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
   if (!success)
   {
      char infoLog[512];
      gl.glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
      BOOST_LOG_TRIVIAL(error)
         << logPrefix_ << "Fragment shader compilation failed: " << infoLog;
   }

   // Create shader program
   p->shaderProgram_ = gl.glCreateProgram();

   gl.glAttachShader(p->shaderProgram_, vertexShader);
   gl.glAttachShader(p->shaderProgram_, fragmentShader);
   gl.glLinkProgram(p->shaderProgram_);

   // Check for errors
   gl.glGetProgramiv(p->shaderProgram_, GL_LINK_STATUS, &success);
   if (!success)
   {
      char infoLog[512];
      gl.glGetProgramInfoLog(p->shaderProgram_, 512, NULL, infoLog);
      BOOST_LOG_TRIVIAL(error)
         << logPrefix_ << "Shader program link failed: " << infoLog;
   }

   // Delete shaders
   gl.glDeleteShader(vertexShader);
   gl.glDeleteShader(fragmentShader);

   // Define 3 (x,y,z) vertices
   GLfloat vertices[] = {
      -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f};

   // Generate a vertex buffer object
   gl.glGenBuffers(1, &p->vbo_);

   // Generate a vertex array object
   gl.glGenVertexArrays(1, &p->vao_);

   // Bind vertex array object
   gl.glBindVertexArray(p->vao_);

   // Copy vertices array in a buffer for OpenGL to use
   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_);
   gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

   // Set the vertex attributes pointers
   gl.glVertexAttribPointer(
      0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), static_cast<void*>(0));
   gl.glEnableVertexAttribArray(0);
}

void TriangleLayer::render(const QMapbox::CustomLayerRenderParameters&)
{
   gl::OpenGLFunctions& gl = p->gl_;

   gl.glUseProgram(p->shaderProgram_);
   gl.glBindVertexArray(p->vao_);
   gl.glDrawArrays(GL_TRIANGLES, 0, 3);
}

void TriangleLayer::deinitialize()
{
   gl::OpenGLFunctions& gl = p->gl_;

   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "deinitialize()";

   gl.glDeleteProgram(p->shaderProgram_);
   gl.glDeleteVertexArrays(1, &p->vao_);
   gl.glDeleteBuffers(1, &p->vbo_);

   p->shaderProgram_ = GL_INVALID_INDEX;
   p->vao_           = GL_INVALID_INDEX;
   p->vbo_           = GL_INVALID_INDEX;
}

} // namespace map
} // namespace qt
} // namespace scwx
