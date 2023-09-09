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
   explicit DrawLayer(const std::shared_ptr<MapContext>& context);
   virtual ~DrawLayer();

   virtual void Initialize() override;
   virtual void
   Render(const QMapLibreGL::CustomLayerRenderParameters&) override;
   virtual void Deinitialize() override;

   virtual bool
   RunMousePicking(const QMapLibreGL::CustomLayerRenderParameters& params,
                   const QPointF&   mouseLocalPos,
                   const QPointF&   mouseGlobalPos,
                   const glm::vec2& mouseCoords) override;

protected:
   void AddDrawItem(const std::shared_ptr<gl::draw::DrawItem>& drawItem);

private:
   std::unique_ptr<DrawLayerImpl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
