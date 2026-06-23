#include <scwx/qt/map/generic_layer.hpp>

namespace scwx::qt::map
{

class GenericLayer::Impl
{
public:
   explicit Impl(std::shared_ptr<render::RenderContext> renderContext) :
       renderContext_ {std::move(renderContext)}
   {
   }

   ~Impl() = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   std::shared_ptr<render::RenderContext> renderContext_;
};

GenericLayer::GenericLayer(
   std::shared_ptr<render::RenderContext> renderContext) :
    p(std::make_unique<Impl>(std::move(renderContext)))
{
}
GenericLayer::~GenericLayer() = default;

bool GenericLayer::RunMousePicking(
   const std::shared_ptr<MapContext>& /* mapContext */,
   const QMapLibre::CustomLayerRenderParameters& /* params */,
   const QPointF& /* mouseLocalPos */,
   const QPointF& /* mouseGlobalPos */,
   const glm::vec2& /* mousePos */,
   const common::Coordinate& /* mouseGeoCoords */,
   std::shared_ptr<types::EventHandler>& /* eventHandler */)
{
   // By default, the layer has nothing to pick
   return false;
}

void GenericLayer::RenderVulkanOverlay(
   QRhiCommandBuffer* /* commandBuffer */,
   render::RhiVulkanOverlayResources& /* resources */,
   const std::shared_ptr<MapContext>& /* mapContext */,
   const QMapLibre::CustomLayerRenderParameters& /* params */)
{
}

std::shared_ptr<render::RenderContext> GenericLayer::render_context() const
{
   return p->renderContext_;
}

} // namespace scwx::qt::map
