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

class PlacefileImages : public DrawItem
{
public:
   explicit PlacefileImages(const std::shared_ptr<GlContext>& context);
   ~PlacefileImages();

   PlacefileImages(const PlacefileImages&)            = delete;
   PlacefileImages& operator=(const PlacefileImages&) = delete;

   PlacefileImages(PlacefileImages&&) noexcept;
   PlacefileImages& operator=(PlacefileImages&&) noexcept;

   void set_selected_time(std::chrono::system_clock::time_point selectedTime);
   void set_thresholded(bool thresholded);

   void Initialize() override;
   void Render(const QMapLibre::CustomLayerRenderParameters& params,
               bool textureAtlasChanged) override;
   void Deinitialize() override;

   /**
    * Resets and prepares the draw item for adding a new set of images.
    */
   void StartImages(const std::string& baseUrl);

   /**
    * Adds a placefile image to the internal draw list.
    *
    * @param [in] di Placefile image
    */
   void AddImage(const std::shared_ptr<gr::Placefile::ImageDrawItem>& di);

   /**
    * Finalizes the draw item after adding new images.
    */
   void FinishImages();

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
