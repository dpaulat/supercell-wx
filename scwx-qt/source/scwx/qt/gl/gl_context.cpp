#include <scwx/qt/gl/gl_context.hpp>
#include <scwx/qt/util/texture_atlas.hpp>
#include <scwx/util/logger.hpp>

#include <algorithm>
#include <boost/container_hash/hash.hpp>
#include <QMessageBox>
#include <QOpenGLContext>

namespace scwx::qt::gl
{

static const std::string logPrefix_ = "scwx::qt::gl::gl_context";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class GlContext::Impl
{
public:
   explicit Impl() = default;
   ~Impl()
   {
      if (layerStateUbo_ != 0 && QOpenGLContext::currentContext() != nullptr)
      {
         glDeleteBuffers(1, &layerStateUbo_);
         layerStateUbo_ = 0;
      }
   }

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void InitializeGL();

   static std::size_t
   GetShaderKey(std::initializer_list<std::pair<GLenum, std::string>> shaders);

   bool glInitialized_ {false};

   std::unordered_map<std::size_t, std::shared_ptr<gl::ShaderProgram>>
              shaderProgramMap_ {};
   std::mutex shaderProgramMutex_ {};

   GLuint     textureAtlas_ {GL_INVALID_INDEX};
   GLuint     layerStateUbo_ {0};
   float      layerOpacity_ {1.0f};
   std::mutex textureMutex_ {};

   std::uint64_t textureBufferCount_ {};
};

GlContext::GlContext() : p(std::make_unique<Impl>()) {}
GlContext::~GlContext() = default;

GlContext::GlContext(GlContext&&) noexcept            = default;
GlContext& GlContext::operator=(GlContext&&) noexcept = default;

std::uint64_t GlContext::texture_buffer_count() const
{
   return p->textureBufferCount_;
}

void GlContext::Impl::InitializeGL()
{
   if (glInitialized_)
   {
      return;
   }

   const int gladVersion = gladLoaderLoadGL();
   if (!gladVersion)
   {
      logger_->error("gladLoaderLoadGL failed");

      QMessageBox::critical(
         nullptr, "Supercell Wx", "Unable to initialize OpenGL");

      throw std::runtime_error("Unable to initialize OpenGL");
   }

   logger_->info("GLAD initialization complete: OpenGL {}.{}",
                 GLAD_VERSION_MAJOR(gladVersion),
                 GLAD_VERSION_MINOR(gladVersion));

   auto glVersion  = reinterpret_cast<const char*>(glGetString(GL_VERSION));
   auto glVendor   = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
   auto glRenderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));

   logger_->info("OpenGL Version: {}", glVersion);
   logger_->info("OpenGL Vendor: {}", glVendor);
   logger_->info("OpenGL Renderer: {}", glRenderer);

   // Get OpenGL version
   GLint major = 0;
   GLint minor = 0;
   glGetIntegerv(GL_MAJOR_VERSION, &major);
   glGetIntegerv(GL_MINOR_VERSION, &minor);

   if (major < 3 || (major == 3 && minor < 3) || !GLAD_GL_VERSION_3_3)
   {
      logger_->error(
         "OpenGL 3.3 or greater is required, found {}.{}", major, minor);

      QMessageBox::critical(
         nullptr,
         "Supercell Wx",
         QString("OpenGL 3.3 or greater is required, found %1.%2\n\n%3\n%4\n%5")
            .arg(major)
            .arg(minor)
            .arg(glVersion)
            .arg(glVendor)
            .arg(glRenderer));

      throw std::runtime_error("OpenGL version too low");
   }

   glGenTextures(1, &textureAtlas_);

   glGenBuffers(1, &layerStateUbo_);
   glBindBuffer(GL_UNIFORM_BUFFER, layerStateUbo_);
   const LayerStateBlock initialBlock {};
   glBufferData(
      GL_UNIFORM_BUFFER, sizeof(initialBlock), &initialBlock, GL_DYNAMIC_DRAW);
   glBindBuffer(GL_UNIFORM_BUFFER, 0);

   glInitialized_ = true;
}

static void BindLayerStateBlock(GLuint programId)
{
   const GLuint index = glGetUniformBlockIndex(programId, kLayerStateBlockName);
   if (index != GL_INVALID_INDEX)
   {
      glUniformBlockBinding(programId, index, kLayerStateBindingPoint);
   }
}

std::shared_ptr<gl::ShaderProgram>
GlContext::GetShaderProgram(const std::string& vertexPath,
                            const std::string& fragmentPath)
{
   return GetShaderProgram(
      {{GL_VERTEX_SHADER, vertexPath}, {GL_FRAGMENT_SHADER, fragmentPath}});
}

std::shared_ptr<gl::ShaderProgram> GlContext::GetShaderProgram(
   std::initializer_list<std::pair<GLenum, std::string>> shaders)
{
   const auto                         key = Impl::GetShaderKey(shaders);
   std::shared_ptr<gl::ShaderProgram> shaderProgram;

   std::unique_lock lock(p->shaderProgramMutex_);

   auto it = p->shaderProgramMap_.find(key);

   if (it == p->shaderProgramMap_.end())
   {
      shaderProgram = std::make_shared<gl::ShaderProgram>();
      shaderProgram->Load(shaders);
      BindLayerStateBlock(shaderProgram->id());
      p->shaderProgramMap_[key] = shaderProgram;
   }
   else
   {
      shaderProgram = it->second;
   }

   return shaderProgram;
}

GLuint GlContext::GetTextureAtlas()
{
   p->InitializeGL();

   std::unique_lock lock(p->textureMutex_);

   auto& textureAtlas = util::TextureAtlas::Instance();

   if (p->textureBufferCount_ != textureAtlas.BuildCount())
   {
      p->textureBufferCount_ = textureAtlas.BuildCount();
      textureAtlas.BufferAtlas(p->textureAtlas_);
   }

   return p->textureAtlas_;
}

void GlContext::set_layer_opacity(float opacity)
{
   p->InitializeGL();
   p->layerOpacity_ = std::clamp(opacity, 0.0f, 1.0f);

   LayerStateBlock block {};
   block.opacity = p->layerOpacity_;

   glBindBuffer(GL_UNIFORM_BUFFER, p->layerStateUbo_);
   glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(block), &block);
   glBindBuffer(GL_UNIFORM_BUFFER, 0);
   glBindBufferBase(
      GL_UNIFORM_BUFFER, kLayerStateBindingPoint, p->layerStateUbo_);
}

float GlContext::layer_opacity() const
{
   return p->layerOpacity_;
}

void GlContext::Initialize()
{
   p->InitializeGL();
}

void GlContext::StartFrame()
{
   glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT);
}

std::size_t GlContext::Impl::GetShaderKey(
   std::initializer_list<std::pair<GLenum, std::string>> shaders)
{
   std::size_t seed = 0;
   for (auto& shader : shaders)
   {
      boost::hash_combine(seed, shader.first);
      boost::hash_combine(seed, shader.second);
   }
   return seed;
}

} // namespace scwx::qt::gl
