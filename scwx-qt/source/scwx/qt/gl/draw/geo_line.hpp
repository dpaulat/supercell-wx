#pragma once

#include <scwx/qt/gl/gl_context.hpp>
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

class GeoLine : public DrawItem
{
public:
   explicit GeoLine(std::shared_ptr<GlContext> context);
   ~GeoLine();

   GeoLine(const GeoLine&)            = delete;
   GeoLine& operator=(const GeoLine&) = delete;

   GeoLine(GeoLine&&) noexcept;
   GeoLine& operator=(GeoLine&&) noexcept;

   void Initialize() override;
   void Render(const QMapbox::CustomLayerRenderParameters& params) override;
   void Deinitialize() override;

   /**
    * Sets the geographic coordinate endpoints associated with the line.
    *
    * @param latitude1 Latitude of the first endpoint in degrees
    * @param longitude1 Longitude of the first endpoint in degrees
    * @param latitude2 Latitude of the second endpoint in degrees
    * @param longitude2 Longitude of the second endpoint in degrees
    */
   void SetPoints(float latitude1,
                  float longitude1,
                  float latitude2,
                  float longitude2);

   /**
    * Sets the modulate color of the line. If specified, the texture color will
    * be multiplied by the modulate color to produce the result.
    *
    * @param color Modulate color (RGBA)
    */
   void SetModulateColor(boost::gil::rgba8_pixel_t color);

   /**
    * Sets the width of the line.
    *
    * @param width Width in pixels
    */
   void SetWidth(float width);

   /**
    * Sets the visibility of the line.
    *
    * @param visible
    */
   void SetVisible(bool visible);

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
