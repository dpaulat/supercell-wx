#pragma once

#include <scwx/qt/gl/gl.hpp>
#include <scwx/qt/gl/shader_program.hpp>

#include <cstdint>

namespace scwx
{
namespace qt
{
namespace gl
{

inline constexpr GLuint      kLayerStateBindingPoint {16};
inline constexpr const char* kLayerStateBlockName {"LayerState"};

struct alignas(16) LayerStateBlock
{
   float opacity {1.0f};
   float pad[3] {};
};

static_assert(sizeof(LayerStateBlock) == 16);

class GlContext
{
public:
   explicit GlContext();
   virtual ~GlContext();

   GlContext(const GlContext&)            = delete;
   GlContext& operator=(const GlContext&) = delete;

   GlContext(GlContext&&) noexcept;
   GlContext& operator=(GlContext&&) noexcept;

   std::uint64_t texture_buffer_count() const;

   std::shared_ptr<gl::ShaderProgram>
   GetShaderProgram(const std::string& vertexPath,
                    const std::string& fragmentPath);
   std::shared_ptr<gl::ShaderProgram> GetShaderProgram(
      std::initializer_list<std::pair<GLenum, std::string>> shaders);

   GLuint GetTextureAtlas();

   void Initialize();
   void StartFrame();

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace gl
} // namespace qt
} // namespace scwx
