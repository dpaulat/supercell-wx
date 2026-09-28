#pragma once

#include <scwx/qt/map/generic_layer.hpp>
#include <scwx/qt/map/grib_frame_info.hpp>

#include <optional>
#include <string>

namespace scwx::qt::map
{

// Renders whatever binary-framed GRIB grid manager::GribManager currently
// has as a MapLibre CustomLayer, using the same LUT/shader colorizing
// approach as RadarProductLayer but with a subdivided-mesh quad instead
// of per-vertex polar geometry (see grib.vert/grib.frag).
class GribProductLayer : public GenericLayer
{
   Q_DISABLE_COPY_MOVE(GribProductLayer)

public:
   explicit GribProductLayer(std::shared_ptr<gl::GlContext> glContext,
                             GribCategory                   category);
   ~GribProductLayer();

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
                   std::shared_ptr<types::EventHandler>& eventHandler) final;

   // The "product\nvalue units\nValid: ..." block for the point under the
   // cursor, or nullopt if there's no data there (out of grid bounds,
   // below noDataThreshold, or no frame loaded yet). GenericLayer's own
   // AddAreaSibling()/CombineAreaHoverText() is what actually combines
   // this with e.g. RadarProductLayer's or another category's
   // GribProductLayer's own text -- see MapWidgetImpl::AddLayer's wiring.
   [[nodiscard]] std::optional<std::string>
   GetHoverText(const std::shared_ptr<MapContext>& mapContext,
                const common::Coordinate& mouseGeoCoords) const override;

private:
   void LoadFrame();
   void BuildPalette();

   // Looks up the raw decoded value nearest (lat, lon), or nullopt if
   // that point falls outside the current grid. Grid-type-aware (see
   // GridType in grib_product_layer.cpp): regular_ll inverts the linear
   // lat1_/lon1_/di_/dj_ mapping directly; lambert reuses the same
   // LambertForward/ComputeLambertConstants the mesh builder already uses,
   // just run in the opposite direction (lat/lon -> grid index instead of
   // grid index -> lat/lon).
   std::optional<float> ValueAt(double lat, double lon) const;

   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map
