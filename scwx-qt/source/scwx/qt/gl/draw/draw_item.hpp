#pragma once

#include <scwx/qt/gl/gl.hpp>

#include <memory>

#include <QMapLibreGL/QMapLibreGL>

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
   virtual void
   Render(const QMapLibreGL::CustomLayerRenderParameters& params) = 0;
   virtual void Deinitialize()                                    = 0;

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
