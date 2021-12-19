#pragma once

#include <scwx/qt/gl/draw/draw_item.hpp>
#include <scwx/qt/map/generic_layer.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

class DrawLayerImpl;

class DrawLayer : public GenericLayer
{
public:
   explicit DrawLayer(std::shared_ptr<MapContext> context);
   virtual ~DrawLayer();

   virtual void Initialize();
   virtual void Render(const QMapbox::CustomLayerRenderParameters&);
   virtual void Deinitialize();

protected:
   void AddDrawItem(std::shared_ptr<gl::draw::DrawItem> drawItem);

private:
   std::unique_ptr<DrawLayerImpl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
