#pragma once

#include <scwx/qt/gl/gl_context.hpp>
#include <scwx/qt/gl/draw/draw_item.hpp>
#include <scwx/gr/placefile.hpp>

namespace scwx::qt::gl::draw
{

class PlacefileImagesXY : public DrawItem
{
public:
   explicit PlacefileImagesXY(const std::shared_ptr<GlContext>& context);
   ~PlacefileImagesXY() override;

   PlacefileImagesXY(const PlacefileImagesXY&)            = delete;
   PlacefileImagesXY& operator=(const PlacefileImagesXY&) = delete;

   PlacefileImagesXY(PlacefileImagesXY&&) noexcept;
   PlacefileImagesXY& operator=(PlacefileImagesXY&&) noexcept;

   void Initialize() override;
   void Render(const QMapLibre::CustomLayerRenderParameters& params,
               bool textureAtlasChanged) override;
   void Deinitialize() override;

   /**
    * Resets and prepares the draw item for adding a new set of images.
    */
   void StartImagesXY(const std::string& baseUrl);

   /**
    * Adds a placefile image to the internal draw list.
    *
    * @param [in] di Placefile image
    */
   void AddImageXY(const std::shared_ptr<gr::Placefile::ImageXYDrawItem>& di);

   /**
    * Finalizes the draw item after adding new images.
    */
   void FinishImagesXY();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::gl::draw
