#pragma once

#include <scwx/qt/gl/gl_context.hpp>
#include <scwx/qt/gl/draw/draw_item.hpp>

#include <boost/gil.hpp>
#include <units/angle.h>

namespace scwx
{
namespace qt
{
namespace gl
{
namespace draw
{

struct GeoIconDrawItem;

class GeoIcons : public DrawItem
{
public:
   explicit GeoIcons(const std::shared_ptr<GlContext>& context);
   ~GeoIcons();

   GeoIcons(const GeoIcons&)            = delete;
   GeoIcons& operator=(const GeoIcons&) = delete;

   GeoIcons(GeoIcons&&) noexcept;
   GeoIcons& operator=(GeoIcons&&) noexcept;

   void set_selected_time(std::chrono::system_clock::time_point selectedTime);
   void set_thresholded(bool thresholded);

   void Initialize() override;
   void Render(const QMapLibreGL::CustomLayerRenderParameters& params,
               bool textureAtlasChanged) override;
   void Deinitialize() override;

   bool RunMousePicking(const QMapLibreGL::CustomLayerRenderParameters& params,
                        const QPointF&            mouseLocalPos,
                        const QPointF&            mouseGlobalPos,
                        const glm::vec2&          mouseCoords,
                        const common::Coordinate& mouseGeoCoords) override;

   /**
    * Sets the visibility of the geo icons.
    *
    * @param [in] visible Icon visibility
    */
   void SetVisible(bool visible);

   /**
    * Resets and prepares the draw item for adding a new set of icon sheets.
    */
   void StartIconSheets();

   /**
    * Adds an icon sheet for drawing the geo icons. The icon sheet must already
    * exist in the texture atlas.
    *
    * @param [in] name The name of the icon sheet in the texture atlas
    * @param [in] iconWidth The width of each icon in the icon sheet. Default is
    * 0 for a single icon.
    * @param [in] iconHeight The height of each icon in the icon sheet. Default
    * is 0 for a single icon.
    * @param [in] hotX The zero-based center of the each icon in the icon sheet.
    * Default is -1 to center the icon.
    * @param [in] hotY The zero-based center of the each icon in the icon sheet.
    * Default is -1 to center the icon.
    */
   void AddIconSheet(const std::string& name,
                     std::size_t        iconWidth  = 0,
                     std::size_t        iconHeight = 0,
                     std::int32_t       hotX       = -1,
                     std::int32_t       hotY       = -1);

   /**
    * Resets and prepares the draw item for adding a new set of icon sheets.
    */
   void FinishIconSheets();

   /**
    * Resets and prepares the draw item for adding a new set of icons.
    */
   void StartIcons();

   /**
    * Adds a geo icon to the internal draw list.
    *
    * @return Geo icon draw item
    */
   std::shared_ptr<GeoIconDrawItem> AddIcon();

   /**
    * Sets the texture of a geo icon.
    *
    * @param [in] di Geo icon draw item
    * @param [in] iconSheet The name of the icon sheet in the texture atlas
    * @param [in] iconIndex The zero-based index of the icon in the icon sheet
    */
   static void SetIconTexture(const std::shared_ptr<GeoIconDrawItem>& di,
                              const std::string&                      iconSheet,
                              std::size_t iconIndex);

   /**
    * Sets the location of a geo icon.
    *
    * @param [in] di Geo icon draw item
    * @param [in] latitude The latitude of the geo icon in degrees.
    * @param [in] longitude The longitude of the geo icon in degrees.
    * @param [in] xOffset The x-offset of the geo icon in pixels. Default is 0.
    * @param [in] yOffset The y-offset of the geo icon in pixels. Default is 0.
    */
   static void SetIconLocation(const std::shared_ptr<GeoIconDrawItem>& di,
                               units::angle::degrees<double>           latitude,
                               units::angle::degrees<double> longitude,
                               double                        xOffset = 0.0,
                               double                        yOffset = 0.0);

   /**
    * Sets the location of a geo icon.
    *
    * @param [in] di Geo icon draw item
    * @param [in] latitude The latitude of the geo icon in degrees.
    * @param [in] longitude The longitude of the geo icon in degrees.
    * @param [in] xOffset The x-offset of the geo icon in pixels. Default is 0.
    * @param [in] yOffset The y-offset of the geo icon in pixels. Default is 0.
    */
   static void SetIconLocation(const std::shared_ptr<GeoIconDrawItem>& di,
                               double                                  latitude,
                               double longitude,
                               double xOffset = 0.0,
                               double yOffset = 0.0);

   /**
    * Sets the angle of a geo icon.
    *
    * @param [in] di Geo icon draw item
    * @param [in] angle Angle in degrees
    */
   static void SetIconAngle(const std::shared_ptr<GeoIconDrawItem>& di,
                            units::angle::degrees<double>           angle);

   /**
    * Sets the modulate color of a geo icon.
    *
    * @param [in] di Geo icon draw item
    * @param [in] modulate Modulate color
    */
   static void SetIconModulate(const std::shared_ptr<GeoIconDrawItem>& di,
                               boost::gil::rgba8_pixel_t modulate);

   /**
    * Sets the modulate color of a geo icon.
    *
    * @param [in] di Geo icon draw item
    * @param [in] modulate Modulate color
    */
   static void SetIconModulate(const std::shared_ptr<GeoIconDrawItem>& di,
                               boost::gil::rgba32f_pixel_t modulate);

   /**
    * Sets the hover text of a geo icon.
    *
    * @param [in] di Geo icon draw item
    * @param [in] text Hover text
    */
   static void SetIconHoverText(const std::shared_ptr<GeoIconDrawItem>& di,
                                const std::string&                      text);

   /**
    * Finalizes the draw item after adding new icons.
    */
   void FinishIcons();

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
