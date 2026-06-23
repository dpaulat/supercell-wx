#pragma once

#include <scwx/awips/phenomenon.hpp>

#include <scwx/qt/map/draw_layer.hpp>
#include <scwx/qt/types/text_event_key.hpp>

#include <memory>

namespace scwx::qt::map
{

class AlertLayerHandler;

class AlertLayer : public DrawLayer
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(AlertLayer)

   friend class AlertLayerHandler;

public:
   explicit AlertLayer(const std::shared_ptr<render::RenderContext>& renderContext,
                       scwx::awips::Phenomenon phenomenon);
   ~AlertLayer();

   void Initialize(const std::shared_ptr<MapContext>& mapContext) final;
   void Render(const std::shared_ptr<MapContext>& mapContext,
               const QMapLibre::CustomLayerRenderParameters&) final;
   void RenderVulkanOverlay(
      QRhiCommandBuffer*                            commandBuffer,
      render::RhiVulkanOverlayResources&            resources,
      const std::shared_ptr<MapContext>&            mapContext,
      const QMapLibre::CustomLayerRenderParameters& params) final;
   void Deinitialize() final;

   static void InitializeHandler();

   [[nodiscard]] static std::size_t SharedGeometrySegmentCount();

signals:
   void AlertSelected(const types::TextEventKey& key);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map
