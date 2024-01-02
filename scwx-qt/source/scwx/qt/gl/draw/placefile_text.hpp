#pragma once

#include <scwx/qt/gl/gl_context.hpp>
#include <scwx/qt/gl/draw/draw_item.hpp>
#include <scwx/qt/types/imgui_font.hpp>
#include <scwx/gr/placefile.hpp>

#include <boost/unordered/unordered_flat_map.hpp>

namespace scwx
{
namespace qt
{
namespace gl
{
namespace draw
{

class PlacefileText : public DrawItem
{
public:
   explicit PlacefileText(const std::shared_ptr<GlContext>& context,
                          const std::string&                placefileName);
   ~PlacefileText();

   PlacefileText(const PlacefileText&)            = delete;
   PlacefileText& operator=(const PlacefileText&) = delete;

   PlacefileText(PlacefileText&&) noexcept;
   PlacefileText& operator=(PlacefileText&&) noexcept;

   void set_placefile_name(const std::string& placefileName);
   void set_selected_time(std::chrono::system_clock::time_point selectedTime);
   void set_thresholded(bool thresholded);

   void Initialize() override;
   void Render(const QMapLibreGL::CustomLayerRenderParameters& params) override;
   void Deinitialize() override;

   bool RunMousePicking(const QMapLibreGL::CustomLayerRenderParameters& params,
                        const QPointF&            mouseLocalPos,
                        const QPointF&            mouseGlobalPos,
                        const glm::vec2&          mouseCoords,
                        const common::Coordinate& mouseGeoCoords) override;

   /**
    * Resets and prepares the draw item for adding a new set of text.
    */
   void StartText();

   /**
    * Configures the fonts for drawing the placefile text.
    *
    * @param [in] fonts A map of ImGui fonts
    */
   void
   SetFonts(const boost::unordered_flat_map<std::size_t,
                                            std::shared_ptr<types::ImGuiFont>>&
               fonts);

   /**
    * Adds placefile text to the internal draw list.
    *
    * @param [in] di Placefile icon
    */
   void AddText(const std::shared_ptr<gr::Placefile::TextDrawItem>& di);

   /**
    * Finalizes the draw item after adding new text.
    */
   void FinishText();

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
