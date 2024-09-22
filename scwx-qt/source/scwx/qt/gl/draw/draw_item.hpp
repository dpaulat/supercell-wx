#pragma once

#include <scwx/qt/gl/gl.hpp>
#include <scwx/qt/types/event_types.hpp>
#include <scwx/common/geographic.hpp>

#include <memory>

#include <glm/gtc/type_ptr.hpp>
#include <qmaplibre.hpp>

namespace scwx
{
namespace qt
{
namespace gl
{
namespace draw
{

class DrawItem
{
public:
   explicit DrawItem(OpenGLFunctions& gl);
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

protected:
   void
   UseDefaultProjection(const QMapLibre::CustomLayerRenderParameters& params,
                        GLint uMVPMatrixLocation);
   void
   UseRotationProjection(const QMapLibre::CustomLayerRenderParameters& params,
                         GLint uMVPMatrixLocation);
   void UseMapProjection(const QMapLibre::CustomLayerRenderParameters& params,
                         GLint uMVPMatrixLocation,
                         GLint uMapScreenCoordLocation);

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
