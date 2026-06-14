#pragma once

#include <scwx/qt/map/generic_layer.hpp>
#include <scwx/qt/map/map_annotation_types.hpp>

#include <units/length.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

namespace scwx::qt::gl::draw
{
class MapAnnotationsDrawItem;
}

namespace scwx::qt::map
{

class MapAnnotationModel;

class MapAnnotationLayer : public GenericLayer
{
   Q_OBJECT

public:
   struct MeasurementOverlay
   {
      std::uint64_t                 id {};
      common::Coordinate            a {};
      common::Coordinate            b {};
      common::Coordinate            labelAnchor {};
      units::length::meters<double> distanceM {};
   };

   explicit MapAnnotationLayer(std::shared_ptr<render::RenderContext> renderContext);
   ~MapAnnotationLayer() override;

   MapAnnotationLayer(const MapAnnotationLayer&)            = delete;
   MapAnnotationLayer& operator=(const MapAnnotationLayer&) = delete;
   MapAnnotationLayer(MapAnnotationLayer&&)                 = delete;
   MapAnnotationLayer& operator=(MapAnnotationLayer&&)      = delete;

   void Initialize(const std::shared_ptr<MapContext>& mapContext) final;
   void Render(const std::shared_ptr<MapContext>& mapContext,
               const QMapLibre::CustomLayerRenderParameters&) final;
   void Deinitialize() final;

#if defined(SCWX_RENDER_BACKEND_VULKAN)
   void RenderVulkanOverlay(
      QRhiCommandBuffer*                            commandBuffer,
      render::RhiVulkanOverlayResources&            resources,
      const std::shared_ptr<MapContext>&            mapContext,
      const QMapLibre::CustomLayerRenderParameters& params) override;
#endif

   bool
   RunMousePicking(const std::shared_ptr<MapContext>&            mapContext,
                   const QMapLibre::CustomLayerRenderParameters& params,
                   const QPointF&                                mouseLocalPos,
                   const QPointF&                                mouseGlobalPos,
                   const glm::vec2&                              mouseCoords,
                   const common::Coordinate&                     mouseGeoCoords,
                   std::shared_ptr<types::EventHandler>& eventHandler) override;

   [[nodiscard]] bool ConsumesLeftDrag() const;
   /// True while left-drag interaction in progress (press established drawing).
   [[nodiscard]] bool IsActivelyDrawing() const;

   void                            SetTool(MapAnnotationTool tool);
   [[nodiscard]] MapAnnotationTool tool() const;

   void                             SetStyle(const MapAnnotationStyle& style);
   [[nodiscard]] MapAnnotationStyle style() const;
   void                             SetVisible(bool visible);
   [[nodiscard]] bool               visible() const;
   void                             CancelInteraction();

   void ClearAll();

   /** Object count; primarily for unit tests. */
   [[nodiscard]] std::size_t GetObjectCount() const;

   void HandleMousePress(const std::shared_ptr<QMapLibre::Map>& map,
                         const QPointF&                         localPos);
   void HandleMouseMove(const std::shared_ptr<QMapLibre::Map>& map,
                        const QPointF&                         localPos);
   void HandleMouseRelease(const std::shared_ptr<QMapLibre::Map>& map,
                           const QPointF&                         localPos);

   [[nodiscard]] std::optional<units::length::meters<double>>
                                                 LastMeasureDistanceM() const;
   [[nodiscard]] std::vector<MeasurementOverlay> GetMeasurementOverlays() const;

signals:
   void MeasureUpdated(double meters);
   void ToolChanged(MapAnnotationTool tool);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map
