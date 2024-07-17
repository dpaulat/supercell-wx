#pragma once

#include <scwx/qt/gl/gl_context.hpp>
#include <scwx/qt/gl/draw/draw_item.hpp>

#include <boost/gil.hpp>

namespace scwx
{
namespace qt
{
namespace gl
{
namespace draw
{

struct GeoLineDrawItem;

class GeoLines : public DrawItem
{
public:
   typedef std::function<void(std::shared_ptr<const GeoLineDrawItem>&,
                              const QPointF&)>
      HoverCallback;

   explicit GeoLines(std::shared_ptr<GlContext> context);
   ~GeoLines();

   GeoLines(const GeoLines&)            = delete;
   GeoLines& operator=(const GeoLines&) = delete;

   GeoLines(GeoLines&&) noexcept;
   GeoLines& operator=(GeoLines&&) noexcept;

   void set_selected_time(std::chrono::system_clock::time_point selectedTime);
   void set_thresholded(bool thresholded);

   void Initialize() override;
   void Render(const QMapLibre::CustomLayerRenderParameters& params) override;
   void Deinitialize() override;

   bool
   RunMousePicking(const QMapLibre::CustomLayerRenderParameters& params,
                   const QPointF&                                mouseLocalPos,
                   const QPointF&                                mouseGlobalPos,
                   const glm::vec2&                              mouseCoords,
                   const common::Coordinate&                     mouseGeoCoords,
                   std::shared_ptr<types::EventHandler>& eventHandler) override;

   /**
    * Sets the visibility of the geo lines.
    *
    * @param [in] visible Line visibility
    */
   void SetVisible(bool visible);

   /**
    * Resets and prepares the draw item for adding a new set of lines.
    */
   void StartLines();

   /**
    * Adds a geo line to the internal draw list.
    *
    * @return Geo line draw item
    */
   std::shared_ptr<GeoLineDrawItem> AddLine();

   /**
    * Sets the location of a geo line.
    *
    * @param [in] di Geo line draw item
    * @param [in] latitude1 The latitude of the first endpoint of the geo line
    * in degrees.
    * @param [in] longitude1 The longitude of the first endpoint of the geo line
    * in degrees.
    * @param [in] latitude2 The latitude of the second endpoint of the geo line
    * in degrees.
    * @param [in] longitude2 The longitude of the second endpoint of the geo
    * line in degrees.
    */
   void SetLineLocation(const std::shared_ptr<GeoLineDrawItem>& di,
                        float                                   latitude1,
                        float                                   longitude1,
                        float                                   latitude2,
                        float                                   longitude2);

   /**
    * Sets the modulate color of a geo line.
    *
    * @param [in] di Geo line draw item
    * @param [in] modulate Modulate color
    */
   void SetLineModulate(const std::shared_ptr<GeoLineDrawItem>& di,
                        boost::gil::rgba8_pixel_t               color);

   /**
    * Sets the modulate color of a geo line.
    *
    * @param [in] di Geo line draw item
    * @param [in] modulate Modulate color
    */
   void SetLineModulate(const std::shared_ptr<GeoLineDrawItem>& di,
                        boost::gil::rgba32f_pixel_t             modulate);

   /**
    * Sets the width of the geo line.
    *
    * @param [in] width Width in pixels
    */
   void SetLineWidth(const std::shared_ptr<GeoLineDrawItem>& di, float width);

   /**
    * Sets the visibility of the geo line.
    *
    * @param [in] visible
    */
   void SetLineVisible(const std::shared_ptr<GeoLineDrawItem>& di,
                       bool                                    visible);

   /**
    * Sets the hover callback enable of a geo line.
    *
    * @param [in] di Geo line draw item
    * @param [in] enabled Hover enabled
    */
   void SetLineHoverCallback(const std::shared_ptr<GeoLineDrawItem>& di,
                             const HoverCallback&                    callback);

   /**
    * Sets the hover text of a geo line.
    *
    * @param [in] di Geo line draw item
    * @param [in] text Hover text
    */
   void SetLineHoverText(const std::shared_ptr<GeoLineDrawItem>& di,
                         const std::string&                      text);

   /**
    * Sets the start time of a geo line.
    *
    * @param [in] di Geo line draw item
    * @param [in] startTime Start time
    */
   void SetLineStartTime(const std::shared_ptr<GeoLineDrawItem>& di,
                         std::chrono::system_clock::time_point   startTime);

   /**
    * Sets the end time of a geo line.
    *
    * @param [in] di Geo line draw item
    * @param [in] endTime End time
    */
   void SetLineEndTime(const std::shared_ptr<GeoLineDrawItem>& di,
                       std::chrono::system_clock::time_point   endTime);

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
