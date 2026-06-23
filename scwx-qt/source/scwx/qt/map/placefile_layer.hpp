#pragma once

#include <scwx/qt/map/draw_layer.hpp>

#include <string>

namespace scwx::qt::map
{

class PlacefileLayer : public DrawLayer
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(PlacefileLayer)

public:
   explicit PlacefileLayer(
      const std::shared_ptr<render::RenderContext>& renderContext,
      const std::string&                            placefileName);
   ~PlacefileLayer();

   std::string placefile_name() const;

   void set_placefile_name(const std::string& placefileName);

   void Initialize(const std::shared_ptr<MapContext>& mapContext) final;
   void Render(const std::shared_ptr<MapContext>& mapContext,
               const QMapLibre::CustomLayerRenderParameters&) final;
   void Deinitialize() final;

#if defined(SCWX_RENDER_BACKEND_VULKAN)
   void RenderVulkanImGui(const std::shared_ptr<MapContext>& mapContext,
                          const QMapLibre::CustomLayerRenderParameters& params);
   void RenderVulkanOverlay(
      QRhiCommandBuffer*                            commandBuffer,
      render::RhiVulkanOverlayResources&            resources,
      const std::shared_ptr<MapContext>&            mapContext,
      const QMapLibre::CustomLayerRenderParameters& params) override;
#endif

   void ReloadData();

signals:
   void DataReloaded();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map
