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
   explicit PlacefileIcons(std::shared_ptr<GlContext> context);
   ~PlacefileIcons();

   PlacefileIcons(const PlacefileIcons&)            = delete;
   PlacefileIcons& operator=(const PlacefileIcons&) = delete;

   PlacefileIcons(PlacefileIcons&&) noexcept;
   PlacefileIcons& operator=(PlacefileIcons&&) noexcept;

   void Initialize() override;
   void Render(const QMapLibreGL::CustomLayerRenderParameters& params) override;
   void Deinitialize() override;

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
    * Resets the list of icons in preparation for rendering a new frame.
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
