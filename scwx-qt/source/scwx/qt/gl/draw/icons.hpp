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

struct IconDrawItem;

class Icons : public DrawItem
{
public:
   explicit Icons(const std::shared_ptr<GlContext>& context);
   ~Icons();

   Icons(const Icons&)            = delete;
   Icons& operator=(const Icons&) = delete;

   Icons(Icons&&) noexcept;
   Icons& operator=(Icons&&) noexcept;

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
    * Sets the visibility of the icons.
    *
    * @param [in] visible Icon visibility
    */
   void SetVisible(bool visible);

   /**
    * Resets and prepares the draw item for adding a new set of icon sheets.
    */
   void StartIconSheets();

   /**
    * Adds an icon sheet for drawing the icons. The icon sheet must already
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
    * Adds an icon to the internal draw list.
    *
    * @return Icon draw item
    */
   std::shared_ptr<IconDrawItem> AddIcon();

   /**
    * Sets the texture of an icon.
    *
    * @param [in] di Icon draw item
    * @param [in] iconSheet The name of the icon sheet in the texture atlas
    * @param [in] iconIndex The zero-based index of the icon in the icon sheet
    */
   static void SetIconTexture(const std::shared_ptr<IconDrawItem>& di,
                              const std::string&                   iconSheet,
                              std::size_t                          iconIndex);

   /**
    * Sets the location of an icon.
    *
    * @param [in] di Icon draw item
    * @param [in] x The x location of the icon in pixels.
    * @param [in] y The y location of the icon in pixels.
    */
   static void
   SetIconLocation(const std::shared_ptr<IconDrawItem>& di, double x, double y);

   /**
    * Sets the angle of an icon.
    *
    * @param [in] di Icon draw item
    * @param [in] angle Angle in degrees
    */
   static void SetIconAngle(const std::shared_ptr<IconDrawItem>& di,
                            units::angle::degrees<double>        angle);

   /**
    * Sets the modulate color of an icon.
    *
    * @param [in] di Icon draw item
    * @param [in] modulate Modulate color
    */
   static void SetIconModulate(const std::shared_ptr<IconDrawItem>& di,
                               boost::gil::rgba8_pixel_t            modulate);

   /**
    * Sets the hover text of an icon.
    *
    * @param [in] di Icon draw item
    * @param [in] text Hover text
    */
   static void SetIconHoverText(const std::shared_ptr<IconDrawItem>& di,
                                const std::string&                   text);

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
