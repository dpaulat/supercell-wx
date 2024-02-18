#pragma once

#include <scwx/qt/gl/gl_context.hpp>
#include <scwx/qt/gl/draw/draw_item.hpp>

#include <boost/gil.hpp>
#include <units/length.h>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class LinkedVectorPacket;

} // namespace rpg
} // namespace wsr88d

namespace qt
{
namespace gl
{
namespace draw
{

struct LinkedVectorDrawItem;

class LinkedVectors : public DrawItem
{
public:
   explicit LinkedVectors(std::shared_ptr<GlContext> context);
   ~LinkedVectors();

   LinkedVectors(const LinkedVectors&)            = delete;
   LinkedVectors& operator=(const LinkedVectors&) = delete;

   LinkedVectors(LinkedVectors&&) noexcept;
   LinkedVectors& operator=(LinkedVectors&&) noexcept;

   void set_selected_time(std::chrono::system_clock::time_point selectedTime);
   void set_thresholded(bool thresholded);

   void Initialize() override;
   void Render(const QMapLibreGL::CustomLayerRenderParameters& params) override;
   void Deinitialize() override;

   bool
   RunMousePicking(const QMapLibreGL::CustomLayerRenderParameters& params,
                   const QPointF&                        mouseLocalPos,
                   const QPointF&                        mouseGlobalPos,
                   const glm::vec2&                      mouseCoords,
                   const common::Coordinate&             mouseGeoCoords,
                   std::shared_ptr<types::EventHandler>& eventHandler) override;

   /**
    * Enables or disables the border around each line in a linked vector.
    *
    * @param [in] enabled Border visibility
    */
   void SetBorderEnabled(bool enabled);

   /**
    * Sets the visibility of the linked vectors.
    *
    * @param [in] visible Line visibility
    */
   void SetVisible(bool visible);

   /**
    * Resets and prepares the draw item for adding a new set of linked vectors.
    */
   void StartVectors();

   /**
    * Adds a linked vector to the internal draw list.
    *
    * @param [in] center Center coordinate on which the linked vectors are based
    * @param [in] vectorPacket Linked vector packet containing start and end
    * points
    *
    * @return Linked vector draw item
    */
   std::shared_ptr<LinkedVectorDrawItem> AddVector(
      const common::Coordinate&                               center,
      const std::shared_ptr<wsr88d::rpg::LinkedVectorPacket>& vectorPacket);

   /**
    * Sets the modulate color of a linked vector.
    *
    * @param [in] di Linked vector draw item
    * @param [in] modulate Modulate color
    */
   static void
   SetVectorModulate(const std::shared_ptr<LinkedVectorDrawItem>& di,
                     boost::gil::rgba8_pixel_t                    color);

   /**
    * Sets the modulate color of a linked vector.
    *
    * @param [in] di Linked vector draw item
    * @param [in] modulate Modulate color
    */
   static void
   SetVectorModulate(const std::shared_ptr<LinkedVectorDrawItem>& di,
                     boost::gil::rgba32f_pixel_t                  modulate);

   /**
    * Sets the width of the linked vector.
    *
    * @param [in] width Width in pixels
    */
   static void SetVectorWidth(const std::shared_ptr<LinkedVectorDrawItem>& di,
                              float width);

   /**
    * Sets the visibility of the linked vector.
    *
    * @param [in] visible
    */
   static void SetVectorVisible(const std::shared_ptr<LinkedVectorDrawItem>& di,
                                bool visible);

   /**
    * Sets the hover text of a linked vector.
    *
    * @param [in] di Linked vector draw item
    * @param [in] text Hover text
    */
   static void
   SetVectorHoverText(const std::shared_ptr<LinkedVectorDrawItem>& di,
                      const std::string&                           text);

   /**
    * Sets the presence of ticks on the linked vector.
    *
    * @param [in] di Linked vector draw item
    * @param [in] enabled Ticks enabled
    */
   static void
   SetVectorTicksEnabled(const std::shared_ptr<LinkedVectorDrawItem>& di,
                         bool                                         enabled);

   /**
    * Sets the tick radius of the linked vector.
    *
    * @param [in] di Linked vector draw item
    * @param [in] radius Length of the tick extending beyond the linked vector
    */
   static void
   SetVectorTickRadius(const std::shared_ptr<LinkedVectorDrawItem>& di,
                       units::length::meters<double>                radius);

   /**
    * Sets the tick radius increment of the linked vector.
    *
    * @param [in] di Linked vector draw item
    * @param [in] radiusIncrement Length increment of each tick beyond the first
    */
   static void
   SetVectorTickRadiusIncrement(const std::shared_ptr<LinkedVectorDrawItem>& di,
                                units::length::meters<double> radiusIncrement);

   /**
    * Finalizes the draw item after adding new linked vectors.
    */
   void FinishVectors();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
