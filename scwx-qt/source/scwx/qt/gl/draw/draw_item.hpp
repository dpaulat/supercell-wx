#pragma once

#include <scwx/qt/types/event_types.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/qt/render/render_context.hpp>

#include <memory>

#include <glm/gtc/type_ptr.hpp>
#include <qmaplibre.hpp>

#if defined(SCWX_RENDER_BACKEND_VULKAN)
class QRhiCommandBuffer;
#endif

namespace scwx
{
namespace qt
{
namespace render
{
struct RhiVulkanOverlayResources;
} // namespace render

namespace gl
{
namespace draw
{

class DrawItem
{
public:
   explicit DrawItem();
   virtual ~DrawItem();

   DrawItem(const DrawItem&)            = delete;
   DrawItem& operator=(const DrawItem&) = delete;

   DrawItem(DrawItem&&) noexcept;
   DrawItem& operator=(DrawItem&&) noexcept;

   virtual void Initialize() = 0;
   virtual void Render(const QMapLibre::CustomLayerRenderParameters& params);
   virtual void Render(const QMapLibre::CustomLayerRenderParameters& params,
                       bool textureAtlasChanged);
   virtual void Deinitialize() = 0;

   virtual void
   RenderVulkan(QRhiCommandBuffer*                            commandBuffer,
                scwx::qt::render::RhiVulkanOverlayResources&  resources,
                const QMapLibre::CustomLayerRenderParameters& params,
                bool textureAtlasChanged);

   /**
    * @brief Run mouse picking on the draw item.
    *
    * @param [in] params Custom layer render parameters
    * @param [in] mouseLocalPos Mouse cursor widget position
    * @param [in] mouseGlobalPos Mouse cursor screen position
    * @param [in] mouseCoords Mouse cursor location in map screen coordinates
    * @param [in] mouseGeoCoords Mouse cursor location in geographic coordinates
    * @param [out] eventHandler Event handler associated with picked draw item
    *
    * @return true if the draw item was picked, otherwise false
    */
   virtual bool
   RunMousePicking(const QMapLibre::CustomLayerRenderParameters& params,
                   const QPointF&                                mouseLocalPos,
                   const QPointF&                                mouseGlobalPos,
                   const glm::vec2&                              mouseCoords,
                   const common::Coordinate&                     mouseGeoCoords,
                   std::shared_ptr<types::EventHandler>&         eventHandler);

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
