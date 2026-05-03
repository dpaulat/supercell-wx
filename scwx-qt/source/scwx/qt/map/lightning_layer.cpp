#include <scwx/qt/map/lightning_layer.hpp>
#include <scwx/provider/blitzortung_data.hpp>
#include <scwx/qt/manager/blitzortung_manager.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/qt/view/radar_product_view.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/qt/gl/draw/geo_icons.hpp>
#include <scwx/qt/types/texture_types.hpp>

#include <algorithm>
#include <chrono>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::lightning_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr auto   kStrikeLifetimeMs_  = std::chrono::milliseconds(4000);
static constexpr float  kCoreBaseOpacity_   = 0.7f;
static constexpr float  kGlowBaseOpacity_   = 0.35f;
static constexpr float  kFlashDurationMs_   = 80.0f;
static constexpr float  kDecayFactor_       = 3.5f;
static constexpr auto   kMinUpdateInterval_ = std::chrono::milliseconds(66);
static constexpr float  kBoundingBoxMetersPerDegree_ = 111320.0f;
static constexpr auto   kOpacityLutStepMs_ = std::chrono::milliseconds(100);
static constexpr double kPi_               = 3.14159265358979323846;

static float LookupLightningIntensity(float ageMs)
{
   if (ageMs < kFlashDurationMs_)
   {
      return 1.0f;
   }

   if (ageMs >= static_cast<float>(kStrikeLifetimeMs_.count()))
   {
      return 0.0f;
   }

   static const auto lut = []()
   {
      constexpr std::size_t kEntries =
         (kStrikeLifetimeMs_.count() / kOpacityLutStepMs_.count()) + 1;

      std::array<float, kEntries> values {};

      for (std::size_t i = 0; i < kEntries; ++i)
      {
         float t = static_cast<float>(i * kOpacityLutStepMs_.count()) /
                   static_cast<float>(kStrikeLifetimeMs_.count());
         values[i] = std::exp(-kDecayFactor_ * t);
      }

      return values;
   }();

   float position = ageMs / static_cast<float>(kOpacityLutStepMs_.count());
   auto  lower    = static_cast<std::size_t>(position);
   auto  upper    = std::min(lower + 1, lut.size() - 1);
   float fraction = position - static_cast<float>(lower);

   return lut[lower] + (lut[upper] - lut[lower]) * fraction;
}

class LightningLayer::Impl
{
public:
   struct StrikeVisual
   {
      manager::TimedStrikeData                   timedStrike_;
      std::shared_ptr<gl::draw::GeoIconDrawItem> coreIcon_;
      std::shared_ptr<gl::draw::GeoIconDrawItem> glowIcon_;
      bool                                       inRange_ {false};
   };

   explicit Impl(LightningLayer*                       self,
                 const std::shared_ptr<gl::GlContext>& glContext) :
       self_(self), geoIcons_(std::make_shared<gl::draw::GeoIcons>(glContext))
   {
   }
   ~Impl() = default;

   Impl(const Impl&)            = delete;
   Impl& operator=(const Impl&) = delete;
   Impl(Impl&&)                 = delete;
   Impl& operator=(Impl&&)      = delete;

   void UpdateStrikes(const std::shared_ptr<MapContext>& mapContext);
   void set_icon_sheets();
   void RebuildStrikeIcons();
   void UpdateRangeCache(const std::shared_ptr<MapContext>& mapContext,
                         double                             centerLat,
                         double                             centerLon,
                         float                              rangeMeters);

   LightningLayer*                       self_;
   std::shared_ptr<gl::draw::GeoIcons>   geoIcons_;
   std::chrono::steady_clock::time_point lastStrikeUpdate_ {};
   std::chrono::steady_clock::time_point lastRenderRequest_ {};
   bool                                  dataDirty_ {true};
   bool                                  animateStrikes_ {false};
   double lastCenterLat_ {std::numeric_limits<double>::quiet_NaN()};
   double lastCenterLon_ {std::numeric_limits<double>::quiet_NaN()};
   float  lastRangeMeters_ {-1.0f};
   std::vector<StrikeVisual> strikeVisuals_ {};
};

void LightningLayer::Impl::set_icon_sheets()
{
   geoIcons_->StartIconSheets();

   geoIcons_->AddIconSheet(
      types::GetTextureName(types::ImageTexture::LightningStrikeCore),
      8,
      8,
      4,
      4);
   geoIcons_->AddIconSheet(
      types::GetTextureName(types::ImageTexture::LightningStrikeGlow),
      24,
      24,
      12,
      12);

   geoIcons_->FinishIconSheets();
}

void LightningLayer::Impl::UpdateStrikes(
   const std::shared_ptr<MapContext>& mapContext)
{
   auto& manager = manager::BlitzortungManager::Instance();

   if (!manager.IsActive())
   {
      geoIcons_->SetVisible(false);
      animateStrikes_ = false;
      return;
   }

   auto radarProductView = mapContext->radar_product_view();
   auto radarSite        = mapContext->radar_site();

   if (!radarSite || !radarProductView || radarProductView->range() <= 0.0f)
   {
      geoIcons_->SetVisible(false);
      animateStrikes_ = false;
      return;
   }

   auto  now         = std::chrono::steady_clock::now();
   float rangeMeters = radarProductView->range() * 1000.0f;

   double centerLat = radarSite->latitude();
   double centerLon = radarSite->longitude();

   if (dataDirty_)
   {
      auto timedStrikes = manager.GetActiveStrikes();

      strikeVisuals_.clear();
      strikeVisuals_.reserve(timedStrikes.size());

      for (const auto& ts : timedStrikes)
      {
         strikeVisuals_.push_back({ts, nullptr, nullptr, false});
      }

      RebuildStrikeIcons();
      lastCenterLat_   = std::numeric_limits<double>::quiet_NaN();
      lastCenterLon_   = std::numeric_limits<double>::quiet_NaN();
      lastRangeMeters_ = -1.0f;
   }

   UpdateRangeCache(mapContext, centerLat, centerLon, rangeMeters);

   std::size_t visibleStrikeCount = 0;

   for (auto& visual : strikeVisuals_)
   {
      auto ageMs = std::chrono::duration<float, std::milli>(
                      now - visual.timedStrike_.receiptTime_)
                      .count();

      if (ageMs >= kStrikeLifetimeMs_.count())
      {
         geoIcons_->SetIconVisible(visual.coreIcon_, false);
         geoIcons_->SetIconVisible(visual.glowIcon_, false);
         continue;
      }

      float intensity = LookupLightningIntensity(ageMs);

      if (!visual.inRange_ || intensity < 0.01f)
      {
         geoIcons_->SetIconVisible(visual.coreIcon_, false);
         geoIcons_->SetIconVisible(visual.glowIcon_, false);
         continue;
      }

      ++visibleStrikeCount;

      geoIcons_->SetIconVisible(visual.coreIcon_, true);
      geoIcons_->SetIconVisible(visual.glowIcon_, true);

      geoIcons_->SetIconModulate(
         visual.coreIcon_,
         boost::gil::rgba32f_pixel_t {
            1.0f, 1.0f, 1.0f, intensity * kCoreBaseOpacity_});
      geoIcons_->SetIconModulate(
         visual.glowIcon_,
         boost::gil::rgba32f_pixel_t {
            0.87f, 0.89f, 1.0f, intensity * kGlowBaseOpacity_});
   }

   geoIcons_->SetVisible(visibleStrikeCount != 0);
   animateStrikes_ = visibleStrikeCount != 0;
   dataDirty_      = false;
}

void LightningLayer::Impl::RebuildStrikeIcons()
{
   geoIcons_->StartIcons();

   for (auto& visual : strikeVisuals_)
   {
      visual.coreIcon_ = geoIcons_->AddIcon();
      geoIcons_->SetIconTexture(
         visual.coreIcon_,
         types::GetTextureName(types::ImageTexture::LightningStrikeCore),
         0);
      geoIcons_->SetIconLocation(visual.coreIcon_,
                                 visual.timedStrike_.strike_.latitude,
                                 visual.timedStrike_.strike_.longitude);
      geoIcons_->SetIconVisible(visual.coreIcon_, false);

      visual.glowIcon_ = geoIcons_->AddIcon();
      geoIcons_->SetIconTexture(
         visual.glowIcon_,
         types::GetTextureName(types::ImageTexture::LightningStrikeGlow),
         0);
      geoIcons_->SetIconLocation(visual.glowIcon_,
                                 visual.timedStrike_.strike_.latitude,
                                 visual.timedStrike_.strike_.longitude);
      geoIcons_->SetIconVisible(visual.glowIcon_, false);
   }

   geoIcons_->FinishIcons();
}

void LightningLayer::Impl::UpdateRangeCache(
   const std::shared_ptr<MapContext>& /* mapContext */,
   double centerLat,
   double centerLon,
   float  rangeMeters)
{
   if (lastCenterLat_ == centerLat && lastCenterLon_ == centerLon &&
       lastRangeMeters_ == rangeMeters)
   {
      return;
   }

   lastCenterLat_   = centerLat;
   lastCenterLon_   = centerLon;
   lastRangeMeters_ = rangeMeters;

   const double latDelta = rangeMeters / kBoundingBoxMetersPerDegree_;
   const double cosLat   = std::max(0.1, std::cos(centerLat * kPi_ / 180.0));
   const double lonDelta = latDelta / cosLat;

   const double minLat = centerLat - latDelta;
   const double maxLat = centerLat + latDelta;
   const double minLon = centerLon - lonDelta;
   const double maxLon = centerLon + lonDelta;

   for (auto& visual : strikeVisuals_)
   {
      const auto& strike = visual.timedStrike_.strike_;

      if (strike.latitude < minLat || strike.latitude > maxLat ||
          strike.longitude < minLon || strike.longitude > maxLon)
      {
         visual.inRange_ = false;
         continue;
      }

      auto distance = util::GeographicLib::GetDistance(
         centerLat, centerLon, strike.latitude, strike.longitude);
      visual.inRange_ = distance <= units::length::meters<double>(rangeMeters);
   }
}

LightningLayer::LightningLayer(
   const std::shared_ptr<gl::GlContext>& glContext) :
    DrawLayer(glContext, "LightningLayer"),
    p(std::make_unique<LightningLayer::Impl>(this, glContext))
{
   AddDrawItem(p->geoIcons_);
}

LightningLayer::~LightningLayer() = default;

void LightningLayer::Initialize(const std::shared_ptr<MapContext>& mapContext)
{
   logger_->debug("Initialize()");
   DrawLayer::Initialize(mapContext);

   p->set_icon_sheets();

   // Start the manager if not already running. Idempotent.
   auto& manager = manager::BlitzortungManager::Instance();
   manager.Start();

   QObject::connect(&manager,
                    &manager::BlitzortungManager::StrikesUpdated,
                    this,
                    [this]()
                    {
                       p->dataDirty_ = true;
                       Q_EMIT NeedsRendering();
                    });

   p->UpdateStrikes(mapContext);
}

void LightningLayer::Render(
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   auto now = std::chrono::steady_clock::now();
   if (p->dataDirty_ || now - p->lastStrikeUpdate_ >= kMinUpdateInterval_)
   {
      p->UpdateStrikes(mapContext);
      p->lastStrikeUpdate_ = now;
   }

   DrawLayer::Render(mapContext, params);

   if (p->animateStrikes_ && now - p->lastRenderRequest_ >= kMinUpdateInterval_)
   {
      p->lastRenderRequest_ = now;
      Q_EMIT NeedsRendering();
   }

   SCWX_GL_CHECK_ERROR();
}

void LightningLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");
   DrawLayer::Deinitialize();
}

} // namespace scwx::qt::map
