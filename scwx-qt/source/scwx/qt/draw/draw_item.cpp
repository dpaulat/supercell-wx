#include <scwx/qt/draw/draw_item.hpp>
#include <scwx/qt/render/rhi_vulkan_overlay.hpp>

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
namespace draw
{

static const std::string logPrefix_ = "scwx::qt::draw::draw_item";

class DrawItem::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;
};

DrawItem::DrawItem() : p(std::make_unique<Impl>()) {}
DrawItem::~DrawItem() = default;

DrawItem::DrawItem(DrawItem&&) noexcept            = default;
DrawItem& DrawItem::operator=(DrawItem&&) noexcept = default;

void DrawItem::Render(
   const QMapLibre::CustomLayerRenderParameters& /* params */)
{
}

void DrawItem::Render(const QMapLibre::CustomLayerRenderParameters& params,
                      bool /* textureAtlasChanged */)
{
   Render(params);
}

void DrawItem::RenderVulkan(
   QRhiCommandBuffer* /* commandBuffer */,
   render::RhiVulkanOverlayResources& /* resources */,
   const QMapLibre::CustomLayerRenderParameters& /* params */,
   bool /* textureAtlasChanged */)
{
}

bool DrawItem::RunMousePicking(
   const QMapLibre::CustomLayerRenderParameters& /* params */,
   const QPointF& /* mouseLocalPos */,
   const QPointF& /* mouseGlobalPos */,
   const glm::vec2& /* mouseCoords */,
   const common::Coordinate& /* mouseGeoCoords */,
   std::shared_ptr<types::EventHandler>& /* eventHandler */)
{
   // By default, the draw item is not picked
   return false;
}

} // namespace draw
} // namespace qt
} // namespace scwx
