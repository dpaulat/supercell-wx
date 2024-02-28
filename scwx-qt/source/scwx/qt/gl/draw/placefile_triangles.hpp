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

class PlacefileTriangles : public DrawItem
{
public:
   explicit PlacefileTriangles(const std::shared_ptr<GlContext>& context);
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
} // namespace gl
} // namespace qt
} // namespace scwx
