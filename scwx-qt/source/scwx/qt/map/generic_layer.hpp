#pragma once

#include <scwx/qt/gl/gl_context.hpp>
#include <scwx/qt/map/map_context.hpp>
#include <scwx/qt/types/event_types.hpp>
#include <scwx/common/geographic.hpp>

#include <memory>
#include <optional>
#include <string>

#include <QObject>
#include <glm/gtc/type_ptr.hpp>
#include <qmaplibre.hpp>

namespace scwx::qt::map
{

class GenericLayer : public QObject
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(GenericLayer)

public:
   explicit GenericLayer(std::shared_ptr<gl::GlContext> glContext);
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

   void                set_opacity(float opacity);
   [[nodiscard]] float opacity() const;

   void BindLayerState();
   void ResetLayerState();

   /**
    * @brief This layer's own Shift-hover data tooltip text for a point, or
    * nullopt if it has nothing to show there. Default returns nullopt --
    * only "area" layers with a meaningful per-point value (radar sweeps,
    * gridded data) need to override this; most layer types (alerts,
    * placefiles, markers -- anything polygon/point-shaped) never will,
    * and get this default for free. See CombineAreaHoverText() below for
    * how multiple such layers combine into one tooltip.
    */
   virtual std::optional<std::string>
   GetHoverText(const std::shared_ptr<MapContext>& mapContext,
                const common::Coordinate&          mouseGeoCoords) const;

   /**
    * @brief Registers another "area" layer (see GetHoverText()) to combine
    * with via CombineAreaHoverText() -- deliberately not automatic/global:
    * only layers explicitly wired together this way combine, so the
    * app's ordinary polygon/marker mouse-picking dispatch (which stops at
    * the first hit -- see MapWidgetImpl::RunMousePicking) is unaffected.
    * Not symmetric on its own -- call it in both directions to actually
    * combine two layers (see MapWidgetImpl::AddLayer's wiring).
    */
   void AddAreaSibling(std::weak_ptr<GenericLayer> sibling);

signals:
   void NeedsRendering();

protected:
   [[nodiscard]] std::shared_ptr<gl::GlContext> gl_context() const;

   /**
    * @brief This layer's own GetHoverText() combined with every sibling
    * added via AddAreaSibling(), separated by a blank line, in whichever
    * order they were added (this layer's own text first). Returns
    * nullopt only if neither this layer nor any sibling has anything to
    * show at this point.
    */
   [[nodiscard]] std::optional<std::string>
   CombineAreaHoverText(const std::shared_ptr<MapContext>& mapContext,
                        const common::Coordinate& mouseGeoCoords) const;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map
