#pragma once

#include <scwx/qt/gl/gl.hpp>
#include <scwx/qt/gl/draw/draw_item.hpp>

#include <boost/gil.hpp>

namespace scwx
{
namespace qt
{
namespace gl
{
namespace draw
{

class RectangleImpl;

class Rectangle : public DrawItem
{
public:
   explicit Rectangle(OpenGLFunctions& gl);
   ~Rectangle();

   Rectangle(const Rectangle&) = delete;
   Rectangle& operator=(const Rectangle&) = delete;

   Rectangle(Rectangle&&) noexcept;
   Rectangle& operator=(Rectangle&&) noexcept;

   void Initialize() override;
   void Render() override;
   void Deinitialize() override;

   void SetBorder(float width, boost::gil::rgba8_pixel_t color);
   void SetFill(boost::gil::rgba8_pixel_t color);
   void SetPosition(float x, float y, float z = 0.0f);
   void SetSize(float width, float height);
   void SetVisible(bool visible);

private:
   std::unique_ptr<RectangleImpl> p;
};

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
