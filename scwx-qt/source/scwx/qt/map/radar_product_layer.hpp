#pragma once

#include <scwx/qt/map/generic_layer.hpp>

#include <optional>

namespace scwx::qt::map
{

class RadarProductLayer : public GenericLayer
{
   Q_DISABLE_COPY_MOVE(RadarProductLayer)

public:
   explicit RadarProductLayer(std::shared_ptr<gl::GlContext> glContext);
   ~RadarProductLayer();

   void Initialize(const std::shared_ptr<MapContext>& mapContext) final;
   void Render(const std::shared_ptr<MapContext>& mapContext,
               const QMapLibre::CustomLayerRenderParameters&) final;
   void Deinitialize() final;

   bool
   RunMousePicking(const std::shared_ptr<MapContext>&            mapContext,
                   const QMapLibre::CustomLayerRenderParameters& params,
                   const QPointF&                                mouseLocalPos,
                   const QPointF&                                mouseGlobalPos,
                   const glm::vec2&                              mouseCoords,
                   const common::Coordinate&                     mouseGeoCoords,
                   std::shared_ptr<types::EventHandler>& eventHandler) override;

   // The distance/altitude-from-radar-site block, plus the bin's own data
   // level code or value if the cursor is over one -- previously built
   // inline in RunMousePicking and shown directly; now returned so
   // GenericLayer::CombineAreaHoverText() can fold it in with e.g. a GRIB
   // layer's own value at the same point.
   [[nodiscard]] std::optional<std::string>
   GetHoverText(const std::shared_ptr<MapContext>& mapContext,
                const common::Coordinate& mouseGeoCoords) const override;

private:
   void UpdateColorTable(const std::shared_ptr<MapContext>& mapContext);
   void UpdateSweep(const std::shared_ptr<MapContext>& mapContext);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map
