#include <scwx/qt/gl/draw/draw_item.hpp>
#include <scwx/qt/util/maplibre.hpp>

#include <string>

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <mbgl/util/constants.hpp>

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

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

void DrawItem::Render(
   const QMapLibreGL::CustomLayerRenderParameters& /* params */)
{
}

void DrawItem::Render(const QMapLibreGL::CustomLayerRenderParameters& params,
                      bool /* textureAtlasChanged */)
{
   Render(params);
}

void DrawItem::UseDefaultProjection(
   const QMapLibreGL::CustomLayerRenderParameters& params,
   GLint                                           uMVPMatrixLocation)
{
   glm::mat4 projection = glm::ortho(0.0f,
                                     static_cast<float>(params.width),
                                     0.0f,
                                     static_cast<float>(params.height));

   p->gl_.glUniformMatrix4fv(
      uMVPMatrixLocation, 1, GL_FALSE, glm::value_ptr(projection));
}

void DrawItem::UseRotationProjection(
   const QMapLibreGL::CustomLayerRenderParameters& params,
   GLint                                           uMVPMatrixLocation)
{
   glm::mat4 projection = glm::ortho(0.0f,
                                     static_cast<float>(params.width),
                                     0.0f,
                                     static_cast<float>(params.height));

   projection = glm::rotate(projection,
                            glm::radians<float>(params.bearing),
                            glm::vec3(0.0f, 0.0f, 1.0f));

   p->gl_.glUniformMatrix4fv(
      uMVPMatrixLocation, 1, GL_FALSE, glm::value_ptr(projection));
}

void DrawItem::UseMapProjection(
   const QMapLibreGL::CustomLayerRenderParameters& params,
   GLint                                           uMVPMatrixLocation,
   GLint                                           uMapScreenCoordLocation)
{
   OpenGLFunctions& gl = p->gl_;

   // TODO: Refactor to utility class
   const float scale = std::pow(2.0, params.zoom) * 2.0f *
                       mbgl::util::tileSize_D / mbgl::util::DEGREES_MAX;
   const float xScale = scale / params.width;
   const float yScale = scale / params.height;

   glm::mat4 uMVPMatrix(1.0f);
   uMVPMatrix = glm::scale(uMVPMatrix, glm::vec3(xScale, yScale, 1.0f));
   uMVPMatrix = glm::rotate(uMVPMatrix,
                            glm::radians<float>(params.bearing),
                            glm::vec3(0.0f, 0.0f, 1.0f));

   gl.glUniform2fv(uMapScreenCoordLocation,
                   1,
                   glm::value_ptr(util::maplibre::LatLongToScreenCoordinate(
                      {params.latitude, params.longitude})));

   gl.glUniformMatrix4fv(
      uMVPMatrixLocation, 1, GL_FALSE, glm::value_ptr(uMVPMatrix));
}

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
