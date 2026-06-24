#pragma once

#include <scwx/qt/draw/draw_item.hpp>

#include <boost/gil.hpp>

class QRhiCommandBuffer;

namespace scwx
{
namespace qt
{
namespace draw
{

class Rectangle : public DrawItem
{
public:
   explicit Rectangle(std::shared_ptr<render::RenderContext> context);
   ~Rectangle();

   Rectangle(const Rectangle&)            = delete;
   Rectangle& operator=(const Rectangle&) = delete;

   Rectangle(Rectangle&&) noexcept;
   Rectangle& operator=(Rectangle&&) noexcept;

   void Initialize() override;
   void Render(const QMapLibre::CustomLayerRenderParameters& params) override;
   void Deinitialize() override;

   void RenderVulkan(QRhiCommandBuffer*                 commandBuffer,
                     render::RhiVulkanOverlayResources& resources,
                     const QMapLibre::CustomLayerRenderParameters& params,
                     bool textureAtlasChanged) override;

   void SetBorder(float width, boost::gil::rgba8_pixel_t color);
   void SetFill(boost::gil::rgba8_pixel_t color);
   void SetPosition(float x, float y, float z = 0.0f);
   void SetSize(float width, float height);
   void SetVisible(bool visible);

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace draw
} // namespace qt
} // namespace scwx
