#pragma once

#include <scwx/qt/gl/gl.hpp>

#include <memory>

#include <QMapLibreGL/QMapLibreGL>
#include <glm/gtc/type_ptr.hpp>

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
   ~DrawItem();

   DrawItem(const DrawItem&)            = delete;
   DrawItem& operator=(const DrawItem&) = delete;

   DrawItem(DrawItem&&) noexcept;
   DrawItem& operator=(DrawItem&&) noexcept;

   virtual void Initialize() = 0;
   virtual void Render(const QMapLibreGL::CustomLayerRenderParameters& params);
   virtual void Render(const QMapLibreGL::CustomLayerRenderParameters& params,
                       bool textureAtlasChanged);
   virtual void Deinitialize() = 0;

   /**
    * @brief Run mouse picking on the draw item.
    *
    * @param [in] params Custom layer render parameters
    * @param [in] mouseLocalPos Mouse cursor widget position
    * @param [in] mouseGlobalPos Mouse cursor screen position
    * @param [in] mouseCoords Mouse cursor location in map screen coordinates
    *
    * @return true if the draw item was picked, otherwise false
    */
   virtual bool
   RunMousePicking(const QMapLibreGL::CustomLayerRenderParameters& params,
                   const QPointF&   mouseLocalPos,
                   const QPointF&   mouseGlobalPos,
                   const glm::vec2& mouseCoords);

protected:
   void
   UseDefaultProjection(const QMapLibreGL::CustomLayerRenderParameters& params,
                        GLint uMVPMatrixLocation);
   void
   UseRotationProjection(const QMapLibreGL::CustomLayerRenderParameters& params,
                         GLint uMVPMatrixLocation);
   void UseMapProjection(const QMapLibreGL::CustomLayerRenderParameters& params,
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
