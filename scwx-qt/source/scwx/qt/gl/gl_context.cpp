#include <scwx/qt/gl/gl_context.hpp>
#include <scwx/qt/util/texture_atlas.hpp>
#include <scwx/util/hash.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace gl
{

static const std::string logPrefix_ = "scwx::qt::gl::gl_context";

class GlContext::Impl
{
public:
   explicit Impl() :
       gl_ {},
       shaderProgramMap_ {},
       shaderProgramMutex_ {},
       textureAtlas_ {GL_INVALID_INDEX},
       textureMutex_ {}
   {
   }
   ~Impl() {}

   gl::OpenGLFunctions gl_;

   std::unordered_map<std::pair<std::string, std::string>,
                      std::shared_ptr<gl::ShaderProgram>,
                      scwx::util::hash<std::pair<std::string, std::string>>>
              shaderProgramMap_;
   std::mutex shaderProgramMutex_;

   GLuint     textureAtlas_;
   std::mutex textureMutex_;
};

GlContext::GlContext() : p(std::make_unique<Impl>()) {}
GlContext::~GlContext() = default;

GlContext::GlContext(GlContext&&) noexcept            = default;
GlContext& GlContext::operator=(GlContext&&) noexcept = default;

gl::OpenGLFunctions& GlContext::gl()
{
   return p->gl_;
}

std::shared_ptr<gl::ShaderProgram>
GlContext::GetShaderProgram(const std::string& vertexPath,
                            const std::string& fragmentPath)
{
   const std::pair<std::string, std::string> key {vertexPath, fragmentPath};
   std::shared_ptr<gl::ShaderProgram>        shaderProgram;

   std::unique_lock lock(p->shaderProgramMutex_);

   auto it = p->shaderProgramMap_.find(key);

   if (it == p->shaderProgramMap_.end())
   {
      shaderProgram = std::make_shared<gl::ShaderProgram>(p->gl_);
      shaderProgram->Load(vertexPath, fragmentPath);
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
   std::unique_lock lock(p->textureMutex_);

   auto& textureAtlas = util::TextureAtlas::Instance();

   if (p->textureAtlas_ == GL_INVALID_INDEX || textureAtlas.NeedsBuffered())
   {
      p->textureAtlas_ = textureAtlas.BufferAtlas(p->gl_);
   }

   return p->textureAtlas_;
}

} // namespace gl
} // namespace qt
} // namespace scwx
