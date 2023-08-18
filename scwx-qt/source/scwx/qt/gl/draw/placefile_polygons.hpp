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

class PlacefilePolygons : public DrawItem
{
public:
   explicit PlacefilePolygons(std::shared_ptr<GlContext> context);
   ~PlacefilePolygons();

   PlacefilePolygons(const PlacefilePolygons&)            = delete;
   PlacefilePolygons& operator=(const PlacefilePolygons&) = delete;

   PlacefilePolygons(PlacefilePolygons&&) noexcept;
   PlacefilePolygons& operator=(PlacefilePolygons&&) noexcept;

   void set_thresholded(bool thresholded);

   void Initialize() override;
   void Render(const QMapLibreGL::CustomLayerRenderParameters& params) override;
   void Deinitialize() override;

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
