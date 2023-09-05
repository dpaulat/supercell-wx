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

class PlacefileLines : public DrawItem
{
public:
   explicit PlacefileLines(const std::shared_ptr<GlContext>& context);
   ~PlacefileLines();

   PlacefileLines(const PlacefileLines&)            = delete;
   PlacefileLines& operator=(const PlacefileLines&) = delete;

   PlacefileLines(PlacefileLines&&) noexcept;
   PlacefileLines& operator=(PlacefileLines&&) noexcept;

   void set_selected_time(std::chrono::system_clock::time_point selectedTime);
   void set_thresholded(bool thresholded);

   void Initialize() override;
   void Render(const QMapLibreGL::CustomLayerRenderParameters& params) override;
   void Deinitialize() override;

   bool RunMousePicking(const QMapLibreGL::CustomLayerRenderParameters& params,
                        const glm::vec2& mousePos) override;

   /**
    * Resets and prepares the draw item for adding a new set of lines.
    */
   void StartLines();

   /**
    * Adds a placefile line to the internal draw list.
    *
    * @param [in] di Placefile line
    */
   void AddLine(const std::shared_ptr<gr::Placefile::LineDrawItem>& di);

   /**
    * Finalizes the draw item after adding new lines.
    */
   void FinishLines();

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
