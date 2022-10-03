#include <scwx/qt/gl/gl_context.hpp>
#include <scwx/util/hash.hpp>

namespace scwx
{
namespace qt
{
namespace gl
{

class GlContext::Impl
{
public:
   explicit Impl() : gl_ {}, shaderProgramMap_ {}, shaderProgramMutex_ {} {}
   ~Impl() {}

   gl::OpenGLFunctions gl_;

   std::unordered_map<std::pair<std::string, std::string>,
                      std::shared_ptr<gl::ShaderProgram>,
                      util::hash<std::pair<std::string, std::string>>>
              shaderProgramMap_;
   std::mutex shaderProgramMutex_;
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

} // namespace gl
} // namespace qt
} // namespace scwx
