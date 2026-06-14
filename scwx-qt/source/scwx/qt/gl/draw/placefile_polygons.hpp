#pragma once

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

class PlacefilePolygons : public DrawItem
{
public:
   explicit PlacefilePolygons(const std::shared_ptr<render::RenderContext>& context);
   ~PlacefilePolygons();

   PlacefilePolygons(const PlacefilePolygons&)            = delete;
   PlacefilePolygons& operator=(const PlacefilePolygons&) = delete;

   PlacefilePolygons(PlacefilePolygons&&) noexcept;
   PlacefilePolygons& operator=(PlacefilePolygons&&) noexcept;

   void set_selected_time(std::chrono::system_clock::time_point selectedTime);
   void set_thresholded(bool thresholded);

   void Initialize() override;
   void Render(const QMapLibre::CustomLayerRenderParameters& params) override;
   void Deinitialize() override;

#if defined(SCWX_RENDER_BACKEND_VULKAN)
   void RenderVulkan(
      QRhiCommandBuffer*                            commandBuffer,
      scwx::qt::render::RhiVulkanOverlayResources&  resources,
      const QMapLibre::CustomLayerRenderParameters& params,
      bool                                          textureAtlasChanged) override;
#endif

   /**
    * Resets and prepares the draw item for adding a new set of polygons.
    */
   void StartPolygons();

   /**
    * Adds a placefile polygon to the internal draw list.
    *
    * @param [in] di Placefile polygon
    */
   void AddPolygon(const std::shared_ptr<gr::Placefile::PolygonDrawItem>& di);

   /**
    * Finalizes the draw item after adding new polygons.
    */
   void FinishPolygons();

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
