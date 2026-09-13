#include <scwx/qt/map/radar_range_layer.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/draw/geo_lines.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/qt/view/radar_product_view.hpp>
#include <scwx/util/logger.hpp>

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::radar_range_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr std::size_t kRangeCircleSegments_ = 120;

static std::vector<std::pair<double, double>>
BuildRangeCircle(float rangeKm, double latitude, double longitude)
{
   std::vector<std::pair<double, double>> points {};
   if (rangeKm <= 0.0f)
   {
      return points;
   }

   const GeographicLib::Geodesic& geodesic(
      util::GeographicLib::DefaultGeodesic());
   const double meters = static_cast<double>(rangeKm) * 1000.0;

   points.reserve(kRangeCircleSegments_);
   for (std::size_t i = 0; i < kRangeCircleSegments_; ++i)
   {
      const double azimuth = (360.0 * static_cast<double>(i)) /
                             static_cast<double>(kRangeCircleSegments_);
      double pointLatitude {};
      double pointLongitude {};
      geodesic.Direct(
         latitude, longitude, azimuth, meters, pointLatitude, pointLongitude);
      points.emplace_back(pointLatitude, pointLongitude);
   }

   return points;
}

class RadarRangeLayer::Impl
{
public:
   explicit Impl(const std::shared_ptr<render::RenderContext>& renderContext) :
       rangeLines_ {std::make_shared<draw::GeoLines>(renderContext)}
   {
   }

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void SetupRangeCircle();
   void UpdateRangeCircle(const std::shared_ptr<MapContext>& mapContext);

   std::shared_ptr<draw::GeoLines>                     rangeLines_;
   std::vector<std::shared_ptr<draw::GeoLineDrawItem>> rangeSegs_ {};
   bool   lastRangeVisible_ {false};
   float  lastRangeKm_ {-1.0f};
   float  lastRangeWidth_ {-1.0f};
   double lastRangeLat_ {0.0};
   double lastRangeLon_ {0.0};
};

RadarRangeLayer::RadarRangeLayer(
   const std::shared_ptr<render::RenderContext>& renderContext) :
    DrawLayer(renderContext, "RadarRangeLayer"),
    p(std::make_unique<Impl>(renderContext))
{
   AddDrawItem(p->rangeLines_);
   p->rangeLines_->set_thresholded(false);
}

RadarRangeLayer::~RadarRangeLayer() = default;

void RadarRangeLayer::Impl::SetupRangeCircle()
{
   rangeLines_->StartLines();
   rangeSegs_.clear();
   rangeSegs_.reserve(kRangeCircleSegments_);

   static const boost::gil::rgba32f_pixel_t kRangeColor {
      0.5f, 0.5f, 0.5f, 0.5f};

   for (std::size_t i = 0; i < kRangeCircleSegments_; ++i)
   {
      auto line = rangeLines_->AddLine();
      rangeLines_->SetLineModulate(line, kRangeColor);
      rangeLines_->SetLineWidth(line, 1.5f);
      rangeLines_->SetLineVisible(line, false);
      rangeSegs_.push_back(std::move(line));
   }

   rangeLines_->FinishLines();
   lastRangeVisible_ = false;
   lastRangeKm_      = -1.0f;
}

void RadarRangeLayer::Impl::UpdateRangeCircle(
   const std::shared_ptr<MapContext>& mapContext)
{
   if (rangeSegs_.empty())
   {
      return;
   }

   const auto radarProductView = mapContext->radar_product_view();
   auto       radarSite        = mapContext->radar_site();
   if (radarSite == nullptr && radarProductView != nullptr)
   {
      const auto manager = radarProductView->radar_product_manager();
      if (manager != nullptr)
      {
         radarSite = manager->radar_site();
      }
   }

   const float rangeKm =
      radarProductView != nullptr ? radarProductView->range() : 0.0f;
   const bool  visible    = radarSite != nullptr && rangeKm > 0.0f;
   const float pixelRatio = mapContext->pixel_ratio();
   const float width      = 1.5f * pixelRatio;

   if (!visible)
   {
      if (lastRangeVisible_)
      {
         for (const auto& seg : rangeSegs_)
         {
            rangeLines_->SetLineVisible(seg, false);
         }
         lastRangeVisible_ = false;
      }
      return;
   }

   const double lat = radarSite->latitude();
   const double lon = radarSite->longitude();

   if (lastRangeVisible_ && lastRangeKm_ == rangeKm && lastRangeLat_ == lat &&
       lastRangeLon_ == lon && lastRangeWidth_ == width)
   {
      return;
   }

   const auto pts = BuildRangeCircle(rangeKm, lat, lon);
   if (pts.size() != rangeSegs_.size())
   {
      return;
   }

   for (std::size_t i = 0; i < rangeSegs_.size(); ++i)
   {
      const auto& a = pts[i];
      const auto& b = pts[(i + 1) % pts.size()];
      rangeLines_->SetLineLocation(rangeSegs_[i],
                                   static_cast<float>(a.first),
                                   static_cast<float>(a.second),
                                   static_cast<float>(b.first),
                                   static_cast<float>(b.second));
      rangeLines_->SetLineWidth(rangeSegs_[i], width);
      rangeLines_->SetLineVisible(rangeSegs_[i], true);
   }

   lastRangeVisible_ = true;
   lastRangeKm_      = rangeKm;
   lastRangeLat_     = lat;
   lastRangeLon_     = lon;
   lastRangeWidth_   = width;
}

void RadarRangeLayer::Initialize(const std::shared_ptr<MapContext>& mapContext)
{
   logger_->debug("Initialize()");

   DrawLayer::Initialize(mapContext);
   p->SetupRangeCircle();
}

void RadarRangeLayer::Render(
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   p->UpdateRangeCircle(mapContext);
   RenderWithoutImGui(params);
}

void RadarRangeLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");

   DrawLayer::Deinitialize();
   p->rangeSegs_.clear();
   p->lastRangeVisible_ = false;
}

void RadarRangeLayer::RenderVulkanOverlay(
   QRhiCommandBuffer*                            commandBuffer,
   render::RhiVulkanOverlayResources&            resources,
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   p->UpdateRangeCircle(mapContext);
   DrawLayer::RenderVulkanOverlay(commandBuffer, resources, mapContext, params);
}

} // namespace scwx::qt::map
