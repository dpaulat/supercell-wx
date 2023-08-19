#pragma once

#include <scwx/qt/gl/gl_context.hpp>
#include <scwx/qt/gl/draw/draw_item.hpp>
#include <scwx/gr/placefile.hpp>

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
   explicit PlacefileText(std::shared_ptr<GlContext> context,
                          const std::string&         placefileName);
   ~PlacefileText();

   PlacefileText(const PlacefileText&)            = delete;
   PlacefileText& operator=(const PlacefileText&) = delete;

   PlacefileText(PlacefileText&&) noexcept;
   PlacefileText& operator=(PlacefileText&&) noexcept;

   void set_placefile_name(const std::string& placefileName);
   void set_thresholded(bool thresholded);

   void Initialize() override;
   void Render(const QMapLibreGL::CustomLayerRenderParameters& params) override;
   void Deinitialize() override;

   /**
    * Adds placefile text to the internal draw list.
    *
    * @param [in] di Placefile icon
    */
   void AddText(const std::shared_ptr<gr::Placefile::TextDrawItem>& di);

   /**
    * Resets the list of text in preparation for rendering a new frame.
    */
   void Reset();

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
