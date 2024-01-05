#pragma once

#include <scwx/qt/map/generic_layer.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

class RadarProductLayerImpl;

class RadarProductLayer : public GenericLayer
{
public:
   explicit RadarProductLayer(std::shared_ptr<MapContext> context);
   ~RadarProductLayer();

   void Initialize() override final;
   void Render(const QMapLibreGL::CustomLayerRenderParameters&) override final;
   void Deinitialize() override final;

   virtual bool
   RunMousePicking(const QMapLibreGL::CustomLayerRenderParameters& params,
                   const QPointF&            mouseLocalPos,
                   const QPointF&            mouseGlobalPos,
                   const glm::vec2&          mouseCoords,
                   const common::Coordinate& mouseGeoCoords) override;

private:
   void UpdateColorTable();
   void UpdateSweep();

private:
   std::unique_ptr<RadarProductLayerImpl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
