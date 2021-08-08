#pragma once

#include <scwx/qt/gl/gl.hpp>

#include <QMapboxGL>

namespace scwx
{
namespace qt
{
namespace map
{

class TriangleLayerImpl;

class TriangleLayer : public QMapbox::CustomLayerHostInterface
{
public:
   explicit TriangleLayer(gl::OpenGLFunctions& gl);
   ~TriangleLayer();

   TriangleLayer(const TriangleLayer&) = delete;
   TriangleLayer& operator=(const TriangleLayer&) = delete;

   TriangleLayer(TriangleLayer&&) noexcept;
   TriangleLayer& operator=(TriangleLayer&&) noexcept;

   void initialize() override final;
   void render(const QMapbox::CustomLayerRenderParameters&) override final;
   void deinitialize() override final;

private:
   std::unique_ptr<TriangleLayerImpl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
