#pragma once

#include <scwx/qt/draw/draw_item.hpp>
#include <scwx/gr/placefile.hpp>

namespace scwx
{
namespace qt
{
namespace draw
{

class PlacefileTriangles : public DrawItem
{
public:
   explicit PlacefileTriangles(
      const std::shared_ptr<render::RenderContext>& context);
   ~PlacefileTriangles();

   PlacefileTriangles(const PlacefileTriangles&)            = delete;
   PlacefileTriangles& operator=(const PlacefileTriangles&) = delete;

   PlacefileTriangles(PlacefileTriangles&&) noexcept;
   PlacefileTriangles& operator=(PlacefileTriangles&&) noexcept;

   void set_selected_time(std::chrono::system_clock::time_point selectedTime);
   void set_thresholded(bool thresholded);

   void Initialize() override;
   void Render(const QMapLibre::CustomLayerRenderParameters& params) override;
   void Deinitialize() override;

   void RenderVulkan(QRhiCommandBuffer*                           commandBuffer,
                     scwx::qt::render::RhiVulkanOverlayResources& resources,
                     const QMapLibre::CustomLayerRenderParameters& params,
                     bool textureAtlasChanged) override;

   /**
    * Resets and prepares the draw item for adding a new set of triangles.
    */
   void StartTriangles();

   /**
    * Adds placefile triangles to the internal draw list.
    *
    * @param [in] di Placefile triangles
    */
   void
   AddTriangles(const std::shared_ptr<gr::Placefile::TrianglesDrawItem>& di);

   /**
    * Finalizes the draw item after adding new triangles.
    */
   void FinishTriangles();

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace draw
} // namespace qt
} // namespace scwx
