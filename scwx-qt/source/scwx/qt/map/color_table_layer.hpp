#pragma once

#include <scwx/qt/map/generic_layer.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

class ColorTableLayerImpl;

class ColorTableLayer : public GenericLayer
{
public:
   explicit ColorTableLayer(std::shared_ptr<MapContext> context);
   ~ColorTableLayer();

   void Initialize() override final;
   void Render(const QMapLibreGL::CustomLayerRenderParameters&) override final;
   void Deinitialize() override final;

private:
   std::unique_ptr<ColorTableLayerImpl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
