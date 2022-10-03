#include <scwx/qt/gl/draw/draw_item.hpp>

#include <string>

#pragma warning(push, 0)
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#pragma warning(pop)

namespace scwx
{
namespace qt
{
namespace gl
{
namespace draw
{

static const std::string logPrefix_ = "scwx::qt::gl::draw::draw_item";

class DrawItem::Impl
{
public:
   explicit Impl(OpenGLFunctions& gl) : gl_ {gl} {}
   ~Impl() {}

   OpenGLFunctions& gl_;
};

DrawItem::DrawItem(OpenGLFunctions& gl) : p(std::make_unique<Impl>(gl)) {}
DrawItem::~DrawItem() = default;

DrawItem::DrawItem(DrawItem&&) noexcept            = default;
DrawItem& DrawItem::operator=(DrawItem&&) noexcept = default;

void DrawItem::UseDefaultProjection(
   const QMapbox::CustomLayerRenderParameters& params, GLint uMVPMatrixLocation)
{
   glm::mat4 projection = glm::ortho(0.0f,
                                     static_cast<float>(params.width),
                                     0.0f,
                                     static_cast<float>(params.height));

   p->gl_.glUniformMatrix4fv(
      uMVPMatrixLocation, 1, GL_FALSE, glm::value_ptr(projection));
}

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
