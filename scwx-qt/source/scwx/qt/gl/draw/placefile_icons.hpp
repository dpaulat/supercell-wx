#pragma once

#include <scwx/qt/gl/gl_context.hpp>
#include <scwx/qt/gl/draw/draw_item.hpp>
#include <scwx/gr/placefile.hpp>

#include <boost/gil.hpp>

namespace scwx
{
namespace qt
{
namespace gl
{
namespace draw
{

class PlacefileIcons : public DrawItem
{
public:
   explicit PlacefileIcons(const std::shared_ptr<GlContext>& context);
   ~PlacefileIcons();

   PlacefileIcons(const PlacefileIcons&)            = delete;
   PlacefileIcons& operator=(const PlacefileIcons&) = delete;

   PlacefileIcons(PlacefileIcons&&) noexcept;
   PlacefileIcons& operator=(PlacefileIcons&&) noexcept;

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
    * Resets and prepares the draw item for adding a new set of icons.
    */
   void StartIcons();

   /**
    * Configures the textures for drawing the placefile icons.
    *
    * @param [in] iconFiles A list of icon files
    * @param [in] baseUrl The base URL of the placefile
    */
   void SetIconFiles(
      const std::vector<std::shared_ptr<const gr::Placefile::IconFile>>&
                         iconFiles,
      const std::string& baseUrl);

   /**
    * Adds a placefile icon to the internal draw list.
    *
    * @param [in] di Placefile icon
    */
   void AddIcon(const std::shared_ptr<gr::Placefile::IconDrawItem>& di);

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
