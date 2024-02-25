#include <scwx/qt/gl/draw/linked_vectors.hpp>
#include <scwx/qt/gl/draw/geo_lines.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/wsr88d/rpg/linked_vector_packet.hpp>

#include <boost/iterator/zip_iterator.hpp>

namespace scwx
{
namespace qt
{
namespace gl
{
namespace draw
{

static const std::string logPrefix_ = "scwx::qt::gl::draw::linked_vectors";

static const boost::gil::rgba32f_pixel_t kBlack {0.0f, 0.0f, 0.0f, 1.0f};

struct LinkedVectorDrawItem
{
   LinkedVectorDrawItem(
      const common::Coordinate& center,
      const std::shared_ptr<const wsr88d::rpg::LinkedVectorPacket>&
         vectorPacket)
   {
      coordinates_.push_back(util::GeographicLib::GetCoordinate(
         center, vectorPacket->start_i_km(), vectorPacket->start_j_km()));

      const auto endI = vectorPacket->end_i_km();
      const auto endJ = vectorPacket->end_j_km();

      std::for_each(
         boost::make_zip_iterator(
            boost::make_tuple(endI.begin(), endJ.begin())),
         boost::make_zip_iterator(boost::make_tuple(endI.end(), endJ.end())),
         [this,
          &center](const boost::tuple<units::length::kilometers<double>,
                                      units::length::kilometers<double>>& p)
         {
            coordinates_.push_back(util::GeographicLib::GetCoordinate(
               center, p.get<0>(), p.get<1>()));
         });
   }

   std::vector<std::shared_ptr<GeoLineDrawItem>> borderDrawItems_ {};
   std::vector<std::shared_ptr<GeoLineDrawItem>> lineDrawItems_ {};

   std::vector<common::Coordinate> coordinates_ {};

   boost::gil::rgba32f_pixel_t modulate_ {1.0f, 1.0f, 1.0f, 1.0f};
   float                       width_ {5.0f};
   bool                        visible_ {true};
   std::string                 hoverText_ {};

   bool                                  ticksEnabled_ {false};
   units::length::nautical_miles<double> tickRadius_ {1.0};
   units::length::nautical_miles<double> tickRadiusIncrement_ {0.0};
};

class LinkedVectors::Impl
{
public:
   explicit Impl(std::shared_ptr<GlContext> context) :
       context_ {context}, geoLines_ {std::make_shared<GeoLines>(context)}
   {
   }

   ~Impl() {}

   std::shared_ptr<GlContext> context_;

   bool borderEnabled_ {true};
   bool visible_ {true};

   std::vector<std::shared_ptr<LinkedVectorDrawItem>> vectorList_ {};
   std::shared_ptr<GeoLines>                          geoLines_;
};

LinkedVectors::LinkedVectors(std::shared_ptr<GlContext> context) :
    DrawItem(context->gl()), p(std::make_unique<Impl>(context))
{
}
LinkedVectors::~LinkedVectors() = default;

LinkedVectors::LinkedVectors(LinkedVectors&&) noexcept            = default;
LinkedVectors& LinkedVectors::operator=(LinkedVectors&&) noexcept = default;

void LinkedVectors::set_selected_time(
   std::chrono::system_clock::time_point selectedTime)
{
   p->geoLines_->set_selected_time(selectedTime);
}

void LinkedVectors::set_thresholded(bool thresholded)
{
   p->geoLines_->set_thresholded(thresholded);
}

void LinkedVectors::Initialize()
{
   p->geoLines_->Initialize();
}

void LinkedVectors::Render(
   const QMapLibreGL::CustomLayerRenderParameters& params)
{
   if (!p->visible_)
   {
      return;
   }

   p->geoLines_->Render(params);
}

void LinkedVectors::Deinitialize()
{
   p->geoLines_->Deinitialize();
}

void LinkedVectors::SetBorderEnabled(bool enabled)
{
   p->borderEnabled_ = enabled;
}

void LinkedVectors::SetVisible(bool visible)
{
   p->visible_ = visible;
}

void LinkedVectors::StartVectors()
{
   // Start a new set of geo lines
   p->geoLines_->StartLines();
   p->vectorList_.clear();
}

std::shared_ptr<LinkedVectorDrawItem> LinkedVectors::AddVector(
   const common::Coordinate&                                     center,
   const std::shared_ptr<const wsr88d::rpg::LinkedVectorPacket>& vectorPacket)
{
   return p->vectorList_.emplace_back(
      std::make_shared<LinkedVectorDrawItem>(center, vectorPacket));
}

void LinkedVectors::SetVectorModulate(
   const std::shared_ptr<LinkedVectorDrawItem>& di,
   boost::gil::rgba8_pixel_t                    modulate)
{
   di->modulate_ = {modulate[0] / 255.0f,
                    modulate[1] / 255.0f,
                    modulate[2] / 255.0f,
                    modulate[3] / 255.0f};
}

void LinkedVectors::SetVectorModulate(
   const std::shared_ptr<LinkedVectorDrawItem>& di,
   boost::gil::rgba32f_pixel_t                  modulate)
{
   di->modulate_ = modulate;
}

void LinkedVectors::SetVectorWidth(
   const std::shared_ptr<LinkedVectorDrawItem>& di, float width)
{
   di->width_ = width;
}

void LinkedVectors::SetVectorVisible(
   const std::shared_ptr<LinkedVectorDrawItem>& di, bool visible)
{
   di->visible_ = visible;
}

void LinkedVectors::SetVectorHoverText(
   const std::shared_ptr<LinkedVectorDrawItem>& di, const std::string& text)
{
   di->hoverText_ = text;
}

void LinkedVectors::SetVectorTicksEnabled(
   const std::shared_ptr<LinkedVectorDrawItem>& di, bool enabled)
{
   di->ticksEnabled_ = enabled;
}

void LinkedVectors::SetVectorTickRadius(
   const std::shared_ptr<LinkedVectorDrawItem>& di,
   units::length::meters<double>                radius)
{
   di->tickRadius_ = radius;
}

void LinkedVectors::SetVectorTickRadiusIncrement(
   const std::shared_ptr<LinkedVectorDrawItem>& di,
   units::length::meters<double>                radiusIncrement)
{
   di->tickRadiusIncrement_ = radiusIncrement;
}

void LinkedVectors::FinishVectors()
{
   // Generate borders
   if (p->borderEnabled_)
   {
      for (auto& di : p->vectorList_)
      {
         auto tickRadius = di->tickRadius_;

         for (std::size_t i = 0; i < di->coordinates_.size() - 1; ++i)
         {
            auto borderLine = p->geoLines_->AddLine();

            const common::Coordinate& coordinate1 = di->coordinates_[i];
            const common::Coordinate& coordinate2 = di->coordinates_[i + 1];

            const double& latitude1  = coordinate1.latitude_;
            const double& longitude1 = coordinate1.longitude_;
            const double& latitude2  = coordinate2.latitude_;
            const double& longitude2 = coordinate2.longitude_;

            GeoLines::SetLineLocation(
               borderLine, latitude1, longitude1, latitude2, longitude2);

            GeoLines::SetLineModulate(borderLine, kBlack);
            GeoLines::SetLineWidth(borderLine, di->width_ + 2.0f);
            GeoLines::SetLineVisible(borderLine, di->visible_);
            GeoLines::SetLineHoverText(borderLine, di->hoverText_);

            di->borderDrawItems_.emplace_back(std::move(borderLine));

            if (di->ticksEnabled_)
            {
               auto angle = util::GeographicLib::GetAngle(
                  latitude1, longitude1, latitude2, longitude2);
               auto angle1 = angle + units::angle::degrees<double>(90.0);
               auto angle2 = angle - units::angle::degrees<double>(90.0);

               auto tickCoord1 = util::GeographicLib::GetCoordinate(
                  coordinate2, angle1, tickRadius);
               auto tickCoord2 = util::GeographicLib::GetCoordinate(
                  coordinate2, angle2, tickRadius);

               const double& tickLat1 = tickCoord1.latitude_;
               const double& tickLon1 = tickCoord1.longitude_;
               const double& tickLat2 = tickCoord2.latitude_;
               const double& tickLon2 = tickCoord2.longitude_;

               auto tickBorderLine = p->geoLines_->AddLine();

               GeoLines::SetLineLocation(
                  tickBorderLine, tickLat1, tickLon1, tickLat2, tickLon2);

               GeoLines::SetLineModulate(tickBorderLine, kBlack);
               GeoLines::SetLineWidth(tickBorderLine, di->width_ + 2.0f);
               GeoLines::SetLineVisible(tickBorderLine, di->visible_);
               GeoLines::SetLineHoverText(tickBorderLine, di->hoverText_);

               tickRadius += di->tickRadiusIncrement_;
            }
         }
      }
   }

   // Generate geo lines
   for (auto& di : p->vectorList_)
   {
      auto tickRadius = di->tickRadius_;

      for (std::size_t i = 0; i < di->coordinates_.size() - 1; ++i)
      {
         auto geoLine = p->geoLines_->AddLine();

         const common::Coordinate& coordinate1 = di->coordinates_[i];
         const common::Coordinate& coordinate2 = di->coordinates_[i + 1];

         const double& latitude1  = coordinate1.latitude_;
         const double& longitude1 = coordinate1.longitude_;
         const double& latitude2  = coordinate2.latitude_;
         const double& longitude2 = coordinate2.longitude_;

         GeoLines::SetLineLocation(
            geoLine, latitude1, longitude1, latitude2, longitude2);

         GeoLines::SetLineModulate(geoLine, di->modulate_);
         GeoLines::SetLineWidth(geoLine, di->width_);
         GeoLines::SetLineVisible(geoLine, di->visible_);

         // If the border is not enabled, this line must have hover text instead
         if (!p->borderEnabled_)
         {
            GeoLines::SetLineHoverText(geoLine, di->hoverText_);
         }

         di->lineDrawItems_.emplace_back(std::move(geoLine));

         if (di->ticksEnabled_)
         {
            auto angle = util::GeographicLib::GetAngle(
               latitude1, longitude1, latitude2, longitude2);
            auto angle1 = angle + units::angle::degrees<double>(90.0);
            auto angle2 = angle - units::angle::degrees<double>(90.0);

            auto tickCoord1 = util::GeographicLib::GetCoordinate(
               coordinate2, angle1, tickRadius);
            auto tickCoord2 = util::GeographicLib::GetCoordinate(
               coordinate2, angle2, tickRadius);

            const double& tickLat1 = tickCoord1.latitude_;
            const double& tickLon1 = tickCoord1.longitude_;
            const double& tickLat2 = tickCoord2.latitude_;
            const double& tickLon2 = tickCoord2.longitude_;

            auto tickGeoLine = p->geoLines_->AddLine();

            GeoLines::SetLineLocation(
               tickGeoLine, tickLat1, tickLon1, tickLat2, tickLon2);

            GeoLines::SetLineModulate(tickGeoLine, di->modulate_);
            GeoLines::SetLineWidth(tickGeoLine, di->width_);
            GeoLines::SetLineVisible(tickGeoLine, di->visible_);

            // If the border is not enabled, this line must have hover text
            if (!p->borderEnabled_)
            {
               GeoLines::SetLineHoverText(tickGeoLine, di->hoverText_);
            }

            tickRadius += di->tickRadiusIncrement_;
         }
      }
   }

   // Finish geo lines
   p->geoLines_->FinishLines();
}

bool LinkedVectors::RunMousePicking(
   const QMapLibreGL::CustomLayerRenderParameters& params,
   const QPointF&                                  mouseLocalPos,
   const QPointF&                                  mouseGlobalPos,
   const glm::vec2&                                mouseCoords,
   const common::Coordinate&                       mouseGeoCoords,
   std::shared_ptr<types::EventHandler>&           eventHandler)
{
   return p->geoLines_->RunMousePicking(params,
                                        mouseLocalPos,
                                        mouseGlobalPos,
                                        mouseCoords,
                                        mouseGeoCoords,
                                        eventHandler);
}

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
