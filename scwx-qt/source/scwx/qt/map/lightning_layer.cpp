#include <scwx/qt/map/lightning_layer.hpp>
#include <scwx/provider/blitzortung_data.hpp>
#include <scwx/qt/manager/blitzortung_manager.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/qt/view/radar_product_view.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/qt/gl/draw/geo_icons.hpp>
#include <scwx/qt/types/texture_types.hpp>

#include <chrono>
#include <cmath>
#include <string>

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::lightning_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr auto  kStrikeLifetimeMs_ = std::chrono::milliseconds(4000);
static constexpr float kCoreBaseOpacity_  = 0.7f;
static constexpr float kGlowBaseOpacity_  = 0.35f;
static constexpr float kFlashDurationMs_  = 80.0f;
static constexpr float kDecayFactor_      = 3.5f;
static constexpr auto  kMinUpdateInterval_ = std::chrono::milliseconds(66);

class LightningLayer::Impl
{
public:
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

   LightningLayer*                     self_;
   std::shared_ptr<gl::draw::GeoIcons> geoIcons_;
   std::chrono::steady_clock::time_point lastStrikeUpdate_ {};
   bool                                  dataDirty_ {true};
   std::size_t                           previousStrikeHash_ {0};
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
      return;
   }

   auto radarProductView = mapContext->radar_product_view();
   auto radarSite        = mapContext->radar_site();

   if (!radarSite || !radarProductView || radarProductView->range() <= 0.0f)
   {
      geoIcons_->SetVisible(false);
      return;
   }

   geoIcons_->SetVisible(true);

   auto  timedStrikes = manager.GetActiveStrikes();
   auto  now          = std::chrono::steady_clock::now();
   float rangeMeters  = radarProductView->range() * 1000.0f;

   double centerLat = radarSite->latitude();
   double centerLon = radarSite->longitude();

   // Compute hash of current strike set for change detection
   std::size_t strikeHash = timedStrikes.size();
   for (const auto& ts : timedStrikes)
   {
      strikeHash ^= static_cast<std::size_t>(ts.strike_.latitude * 10000.0) +
                    static_cast<std::size_t>(ts.strike_.longitude * 10000.0) +
                    static_cast<std::size_t>(ts.strike_.time_ns);
   }

   if (strikeHash == previousStrikeHash_)
   {
      // Strike set unchanged from last update, skip full rebuild
      // Opacity freeze for one interval is imperceptible
      geoIcons_->SetVisible(true);
      return;
   }
   previousStrikeHash_ = strikeHash;

   geoIcons_->StartIcons();

   for (const auto& ts : timedStrikes)
   {
      auto ageMs =
         std::chrono::duration<float, std::milli>(now - ts.receiptTime_)
            .count();

      if (ageMs >= kStrikeLifetimeMs_.count())
      {
         continue;
      }

      float intensity;
      if (ageMs < kFlashDurationMs_)
      {
         intensity = 1.0f;
      }
      else
      {
         float t   = ageMs / static_cast<float>(kStrikeLifetimeMs_.count());
         intensity = std::exp(-kDecayFactor_ * t);
      }

      if (intensity < 0.01f)
      {
         continue;
      }

      // Skip strikes outside the radar range ring
      auto distance = util::GeographicLib::GetDistance(
         centerLat, centerLon, ts.strike_.latitude, ts.strike_.longitude);
      if (distance > units::length::meters<double>(rangeMeters))
      {
         continue;
      }

      // Core pass: white
      {
         float opacity = intensity * kCoreBaseOpacity_;
         auto  icon    = geoIcons_->AddIcon();
         geoIcons_->SetIconTexture(
            icon,
            types::GetTextureName(types::ImageTexture::LightningStrikeCore),
            0);
         geoIcons_->SetIconLocation(
            icon, ts.strike_.latitude, ts.strike_.longitude);
         geoIcons_->SetIconModulate(
            icon, boost::gil::rgba32f_pixel_t {1.0f, 1.0f, 1.0f, opacity});
      }

      // Glow pass: blue-white
      {
         float opacity = intensity * kGlowBaseOpacity_;
         auto  icon    = geoIcons_->AddIcon();
         geoIcons_->SetIconTexture(
            icon,
            types::GetTextureName(types::ImageTexture::LightningStrikeGlow),
            0);
         geoIcons_->SetIconLocation(
            icon, ts.strike_.latitude, ts.strike_.longitude);
         geoIcons_->SetIconModulate(
            icon, boost::gil::rgba32f_pixel_t {0.87f, 0.89f, 1.0f, opacity});
      }
   }

   geoIcons_->FinishIcons();
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
      p->dataDirty_        = false;
   }

   DrawLayer::Render(mapContext, params);

   SCWX_GL_CHECK_ERROR();
}

void LightningLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");
   DrawLayer::Deinitialize();
}

} // namespace scwx::qt::map
