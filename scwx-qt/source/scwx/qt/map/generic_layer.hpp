#pragma once

#include <scwx/qt/map/map_context.hpp>
#include <scwx/qt/render/render_context.hpp>
#include <scwx/qt/types/event_types.hpp>
#include <scwx/common/geographic.hpp>

#include <memory>

#include <QObject>
#include <glm/gtc/type_ptr.hpp>
#include <qmaplibre.hpp>

class QRhi;
class QRhiCommandBuffer;
class QRhiRenderTarget;

namespace scwx::qt::render
{

class RhiVulkanOverlayResources;

} // namespace scwx::qt::render

namespace scwx::qt::map
{

class GenericLayer : public QObject
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(GenericLayer)

public:
   explicit GenericLayer(std::shared_ptr<render::RenderContext> renderContext);
   virtual ~GenericLayer();

   virtual void Initialize(const std::shared_ptr<MapContext>& mapContext) = 0;
   virtual void Render(const std::shared_ptr<MapContext>& mapContext,
                       const QMapLibre::CustomLayerRenderParameters&)     = 0;
   virtual void Deinitialize()                                            = 0;

   /**
    * @brief Run mouse picking on the layer.
    *
    * @param [in] mapContext Map context
    * @param [in] params Custom layer render parameters
    * @param [in] mouseLocalPos Mouse cursor widget position
    * @param [in] mouseGlobalPos Mouse cursor screen position
    * @param [in] mouseCoords Mouse cursor location in map screen coordinates
    * @param [in] mouseGeoCoords Mouse cursor location in geographic coordinates
    * @param [out] eventHandler Event handler associated with picked draw item
    *
    * @return true if a draw item was picked, otherwise false
    */
   virtual bool
   RunMousePicking(const std::shared_ptr<MapContext>&            mapContext,
                   const QMapLibre::CustomLayerRenderParameters& params,
                   const QPointF&                                mouseLocalPos,
                   const QPointF&                                mouseGlobalPos,
                   const glm::vec2&                              mouseCoords,
                   const common::Coordinate&                     mouseGeoCoords,
                   std::shared_ptr<types::EventHandler>&         eventHandler);

   virtual void
   RenderVulkanOverlay(QRhiCommandBuffer*                 commandBuffer,
                       render::RhiVulkanOverlayResources& resources,
                       const std::shared_ptr<MapContext>& mapContext,
                       const QMapLibre::CustomLayerRenderParameters& params);

signals:
   void NeedsRendering();

protected:
   [[nodiscard]] std::shared_ptr<render::RenderContext> render_context() const;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map
