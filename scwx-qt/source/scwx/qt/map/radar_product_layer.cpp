#include <scwx/qt/map/radar_product_layer.hpp>
#include <scwx/qt/map/map_settings.hpp>
#include <scwx/qt/settings/unit_settings.hpp>
#include <scwx/qt/types/unit_types.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/qt/util/tooltip.hpp>
#include <scwx/qt/view/radar_product_view.hpp>
#include <scwx/util/logger.hpp>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>

#if defined(SCWX_RENDER_BACKEND_VULKAN)
#   include <scwx/qt/render/rhi_radar_overlay.hpp>
#   include <scwx/qt/render/rhi_vulkan_overlay.hpp>
#endif

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#include <boost/algorithm/string.hpp>
#include <boost/timer/timer.hpp>
#include <fmt/format.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <mbgl/util/constants.hpp>
#include <QGuiApplication>

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::radar_product_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

namespace
{

std::size_t MomentDataByteSize(const std::size_t numVertices,
                               const std::size_t componentSize)
{
   if (numVertices == 0)
   {
      return 0;
   }

   const std::size_t componentBytes = std::max<std::size_t>(1, componentSize);
   return numVertices * componentBytes;
}

bool HasCompleteMomentData(const std::size_t                numVertices,
                           const std::size_t                componentSize,
                           const std::vector<std::uint8_t>& momentData)
{
   return momentData.size() >= MomentDataByteSize(numVertices, componentSize);
}

} // namespace

class RadarProductLayer::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   bool cfpEnabled_ {false};

   std::uint16_t rangeMin_ {0};
   float         scale_ {1.0f};

   bool colorTableNeedsUpdate_ {false};
   bool sweepNeedsUpdate_ {false};
   bool colorTableUploadNeeded_ {true};
   bool sweepUploadNeeded_ {true};

   std::vector<float>        vertices_ {};
   std::vector<std::uint8_t> momentData_ {};
   std::vector<std::uint8_t> cfpData_ {};
   std::size_t               momentComponentSize_ {0};
   std::size_t               cfpComponentSize_ {0};
   std::vector<std::uint8_t> rgbaColorTable_ {};
   std::size_t               numVertices_ {0};

   types::RadarProductLoadStatus latchedLoadStatus_ {
      types::RadarProductLoadStatus::ProductNotAvailable};
};

RadarProductLayer::RadarProductLayer(
   std::shared_ptr<render::RenderContext> renderContext) :
    GenericLayer(std::move(renderContext)), p(std::make_unique<Impl>())
{
}
RadarProductLayer::~RadarProductLayer() = default;

void RadarProductLayer::Initialize(
   const std::shared_ptr<MapContext>& mapContext)
{
   logger_->debug("Initialize()");

   p->sweepNeedsUpdate_ = true;
   UpdateSweep(mapContext);
   p->colorTableNeedsUpdate_ = true;
   UpdateColorTable(mapContext);

   auto radarProductView = mapContext->radar_product_view();
   if (radarProductView == nullptr)
   {
      return;
   }

   connect(radarProductView.get(),
           &view::RadarProductView::ColorTableLutUpdated,
           this,
           [this]()
           {
              p->colorTableNeedsUpdate_ = true;
              Q_EMIT NeedsRendering();
           });
   connect(radarProductView.get(),
           &view::RadarProductView::SweepComputed,
           this,
           [this]()
           {
              p->sweepNeedsUpdate_ = true;
              Q_EMIT NeedsRendering();
           });
   connect(radarProductView.get(),
           &view::RadarProductView::SweepNotComputed,
           this,
           [this](types::NoUpdateReason reason)
           {
              if (reason == types::NoUpdateReason::NotAvailable)
              {
                 // Ensure the radar product is hidden by re-rendering
                 Q_EMIT NeedsRendering();
              }
              if (reason == types::NoUpdateReason::NoChange)
              {
                 if (p->latchedLoadStatus_ ==
                     types::RadarProductLoadStatus::ProductNotAvailable)
                 {
                    // Ensure the radar product is shown by re-rendering
                    Q_EMIT NeedsRendering();
                 }
              }
           });
}

void RadarProductLayer::UpdateSweep(
   const std::shared_ptr<MapContext>& mapContext)
{
   // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
   // NOLINTBEGIN(modernize-use-nullptr)

   std::shared_ptr<view::RadarProductView> radarProductView =
      mapContext->radar_product_view();
   if (radarProductView == nullptr)
   {
      return;
   }

   std::unique_lock sweepLock(radarProductView->sweep_mutex(),
                              std::try_to_lock);
   if (!sweepLock.owns_lock())
   {
      logger_->trace("Sweep locked, deferring update");
      return;
   }
   logger_->debug("UpdateSweep()");

   p->sweepNeedsUpdate_ = false;

   const std::vector<float>& vertices = radarProductView->vertices();
   p->vertices_                       = vertices;

   const void* data {};
   std::size_t dataSize {};
   std::size_t componentSize {};

   std::tie(data, dataSize, componentSize) = radarProductView->GetMomentData();
   p->momentData_.resize(dataSize);
   if (dataSize > 0 && data != nullptr)
   {
      std::memcpy(p->momentData_.data(), data, dataSize);
   }
   p->momentComponentSize_ = componentSize;

   const void* cfpData {};
   std::size_t cfpDataSize {};
   std::size_t cfpComponentSize {};

   std::tie(cfpData, cfpDataSize, cfpComponentSize) =
      radarProductView->GetCfpMomentData();
   p->cfpData_.resize(cfpDataSize);
   if (cfpDataSize > 0 && cfpData != nullptr)
   {
      std::memcpy(p->cfpData_.data(), cfpData, cfpDataSize);
   }
   p->cfpComponentSize_ = cfpComponentSize;

   p->numVertices_       = vertices.size() / 2;
   p->sweepUploadNeeded_ = true;

   // NOLINTEND(modernize-use-nullptr)
   // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
}

void RadarProductLayer::Render(
   const std::shared_ptr<MapContext>& /* mapContext */,
   const QMapLibre::CustomLayerRenderParameters& /* params */)
{
}

#if defined(SCWX_RENDER_BACKEND_VULKAN)
void RadarProductLayer::RenderVulkanOverlay(
   QRhiCommandBuffer*                            commandBuffer,
   render::RhiVulkanOverlayResources&            resources,
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   if (p->colorTableNeedsUpdate_)
   {
      UpdateColorTable(mapContext);
   }

   if (p->sweepNeedsUpdate_)
   {
      UpdateSweep(mapContext);
   }

   const std::shared_ptr<view::RadarProductView> radarProductView =
      mapContext->radar_product_view();

   bool                          sweepVisible = false;
   types::RadarProductLoadStatus newLoadStatus =
      types::RadarProductLoadStatus::ProductNotLoaded;

   if (radarProductView != nullptr)
   {
      newLoadStatus = radarProductView->load_status();

      switch (newLoadStatus)
      {
      case types::RadarProductLoadStatus::ProductNotAvailable:
         sweepVisible = false;
         break;

      case types::RadarProductLoadStatus::ListingProducts:
         sweepVisible = p->latchedLoadStatus_ !=
                        types::RadarProductLoadStatus::ProductNotAvailable;
         break;

      case types::RadarProductLoadStatus::ProductLoaded:
         sweepVisible = true;
         break;

      case types::RadarProductLoadStatus::ProductNotLoaded:
      case types::RadarProductLoadStatus::LoadingProduct:
         sweepVisible = false;
         break;

      default:
         sweepVisible = false;
         break;
      }
   }

   if (sweepVisible && !HasCompleteMomentData(p->numVertices_,
                                              p->momentComponentSize_,
                                              p->momentData_))
   {
      sweepVisible = false;
   }

   if (sweepVisible && p->numVertices_ > 0 && !p->rgbaColorTable_.empty())
   {
      const int tableWidth = static_cast<int>(p->rgbaColorTable_.size() / 4u);
      if (tableWidth > 0 && p->scale_ > 0.0f)
      {
         render::RadarUniforms uniforms {};
         const glm::vec2       mapScale = util::maplibre::GetMapScale(params);
         uniforms.uMVPMatrix            = glm::mat4 {1.0f};
         uniforms.uMapMatrix            = glm::scale(
            glm::mat4 {1.0f}, glm::vec3(mapScale.x, -mapScale.y, 1.0f));
         uniforms.uMapMatrix =
            glm::rotate(uniforms.uMapMatrix,
                        glm::radians<float>(static_cast<float>(params.bearing)),
                        glm::vec3(0.0f, 0.0f, 1.0f));
         uniforms.uOriginLatLong =
            glm::vec2 {static_cast<float>(params.latitude),
                       static_cast<float>(params.longitude)};
         uniforms.uDataMomentOffset = p->rangeMin_;
         uniforms.uDataMomentScale  = p->scale_;
         uniforms.uCFPEnabled       = p->cfpEnabled_ ? 1 : 0;

         resources.radar.Render(commandBuffer,
                                uniforms,
                                p->vertices_,
                                p->momentData_,
                                p->momentComponentSize_,
                                p->cfpData_,
                                p->cfpComponentSize_,
                                p->rgbaColorTable_,
                                static_cast<std::uint32_t>(p->numVertices_),
                                p->sweepUploadNeeded_,
                                p->colorTableUploadNeeded_);
         p->sweepUploadNeeded_      = false;
         p->colorTableUploadNeeded_ = false;

         if (std::getenv("SCWX_VULKAN_SMOKE") != nullptr)
         {
            static auto lastLog = std::chrono::steady_clock::now();
            const auto  now     = std::chrono::steady_clock::now();
            if (now - lastLog > std::chrono::seconds {3})
            {
               logger_->info("Vulkan radar draw: verts={}", p->numVertices_);
               lastLog = now;
            }
         }
      }
   }
   else if (std::getenv("SCWX_VULKAN_SMOKE") != nullptr)
   {
      static auto lastLog = std::chrono::steady_clock::now();
      const auto  now     = std::chrono::steady_clock::now();
      if (now - lastLog > std::chrono::seconds {3})
      {
         logger_->info(
            "Vulkan radar skip: visible={} verts={} lut={} status={}",
            sweepVisible,
            p->numVertices_,
            p->rgbaColorTable_.size(),
            static_cast<int>(newLoadStatus));
         lastLog = now;
      }
   }

   if (radarProductView != nullptr &&
       !(p->latchedLoadStatus_ ==
            types::RadarProductLoadStatus::ProductNotAvailable &&
         newLoadStatus == types::RadarProductLoadStatus::ListingProducts))
   {
      p->latchedLoadStatus_ = newLoadStatus;
   }
}
#endif

void RadarProductLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");

   p->vertices_.clear();
   p->momentData_.clear();
   p->cfpData_.clear();
   p->rgbaColorTable_.clear();
   p->numVertices_ = 0;
}

bool RadarProductLayer::RunMousePicking(
   const std::shared_ptr<MapContext>& mapContext,
   const QMapLibre::CustomLayerRenderParameters& /* params */,
   const QPointF& /* mouseLocalPos */,
   const QPointF& mouseGlobalPos,
   const glm::vec2& /* mouseCoords */,
   const common::Coordinate& mouseGeoCoords,
   std::shared_ptr<types::EventHandler>& /* eventHandler */)
{
   bool itemPicked = false;

   if (QGuiApplication::keyboardModifiers() &
       Qt::KeyboardModifier::ShiftModifier)
   {
      std::shared_ptr<view::RadarProductView> radarProductView =
         mapContext->radar_product_view();

      if (mapContext->radar_site() == nullptr)
      {
         return itemPicked;
      }

      // Get distance and altitude of point
      const double radarLatitude  = mapContext->radar_site()->latitude();
      const double radarLongitude = mapContext->radar_site()->longitude();

      const auto distanceMeters =
         util::GeographicLib::GetDistance(mouseGeoCoords.latitude_,
                                          mouseGeoCoords.longitude_,
                                          radarLatitude,
                                          radarLongitude);

      const std::string distanceUnitName =
         settings::UnitSettings::Instance().distance_units().GetValue();
      const types::DistanceUnits distanceUnits =
         types::GetDistanceUnitsFromName(distanceUnitName);
      const double distanceScale = types::GetDistanceUnitsScale(distanceUnits);
      const std::string distanceAbbrev =
         types::GetDistanceUnitsAbbreviation(distanceUnits);

      const double distance = distanceMeters.value() *
                              scwx::common::kKilometersPerMeter * distanceScale;
      std::string distanceHeightStr =
         fmt::format("{:.2f} {}", distance, distanceAbbrev);

      if (radarProductView == nullptr)
      {
         util::tooltip::Show(distanceHeightStr, mouseGlobalPos);
         itemPicked = true;
         return itemPicked;
      }

      std::optional<float> elevation = radarProductView->elevation();
      if (elevation.has_value())
      {
         const auto altitudeMeters =
            util::GeographicLib::GetRadarBeamAltititude(
               distanceMeters,
               units::angle::degrees<double>(*elevation),
               mapContext->radar_site()->altitude());

         const std::string heightUnitName =
            settings::UnitSettings::Instance().echo_tops_units().GetValue();
         const types::EchoTopsUnits heightUnits =
            types::GetEchoTopsUnitsFromName(heightUnitName);
         const double heightScale = types::GetEchoTopsUnitsScale(heightUnits);
         const std::string heightAbbrev =
            types::GetEchoTopsUnitsAbbreviation(heightUnits);

         const double altitude = altitudeMeters.value() *
                                 scwx::common::kKilometersPerMeter *
                                 heightScale;

         distanceHeightStr = fmt::format(
            "{}\n{:.2f} {}", distanceHeightStr, altitude, heightAbbrev);
      }

      std::optional<std::uint16_t> binLevel =
         radarProductView->GetBinLevel(mouseGeoCoords);

      if (binLevel.has_value())
      {
         // Hovering over a bin on the map
         std::optional<wsr88d::DataLevelCode> code =
            radarProductView->GetDataLevelCode(binLevel.value());
         std::optional<float> value =
            radarProductView->GetDataValue(binLevel.value());

         if (code.has_value() && //
             code.value() != wsr88d::DataLevelCode::Blank &&
             code.value() != wsr88d::DataLevelCode::NoData &&
             code.value() != wsr88d::DataLevelCode::Topped)
         {
            // Level has associated data level code
            std::string codeName = wsr88d::GetDataLevelCodeName(code.value());
            std::string codeShortName =
               wsr88d::GetDataLevelCodeShortName(code.value());
            std::string hoverText;

            if (codeName != codeShortName && !codeShortName.empty())
            {
               // There is a unique long and short name for the code
               hoverText = fmt::format(
                  "{}: {}\n{}", codeShortName, codeName, distanceHeightStr);
            }
            else
            {
               // Otherwise, only use the long name (always present)
               hoverText = fmt::format("{}\n{}", codeName, distanceHeightStr);
            }

            // Show the tooltip
            util::tooltip::Show(hoverText, mouseGlobalPos);

            itemPicked = true;
         }
         else if (value.has_value())
         {
            // Level has associated data value
            float       f = value.value();
            std::string units {};
            std::string suffix {};
            std::string hoverText;

            // Determine units from radar product view
            units = radarProductView->units();
            if (!units.empty())
            {
               f = f * radarProductView->unit_scale();
            }
            else
            {
               std::shared_ptr<common::ColorTable> colorTable =
                  radarProductView->color_table();

               if (colorTable != nullptr)
               {
                  // Scale data value according to the color table, and get
                  // units
                  f     = f * colorTable->scale() + colorTable->offset();
                  units = colorTable->units();
               }
            }

            if (code.has_value() &&
                code.value() == wsr88d::DataLevelCode::Topped)
            {
               // Show " TOPPED" suffix for echo tops
               suffix = " TOPPED";
            }

            if (units.empty() ||          //
                units.starts_with("?") || //
                boost::iequals(units, "NONE") ||
                boost::iequals(units, "UNITLESS") ||
                radarProductView->IgnoreUnits())
            {
               // Don't display a units value that wasn't intended to be
               // displayed
               hoverText =
                  fmt::format("{}{}\n{}", f, suffix, distanceHeightStr);
            }
            else if (std::isalpha(static_cast<unsigned char>(units.at(0))))
            {
               // dBZ, Kts, etc.
               hoverText = fmt::format(
                  "{} {}{}\n{}", f, units, suffix, distanceHeightStr);
            }
            else
            {
               // %, etc.
               hoverText = fmt::format(
                  "{}{}{}\n{}", f, units, suffix, distanceHeightStr);
            }

            // Show the tooltip
            util::tooltip::Show(hoverText, mouseGlobalPos);

            itemPicked = true;
         }
      }
      else
      {
         // Always show tooltip for distance and altitude
         util::tooltip::Show(distanceHeightStr, mouseGlobalPos);
         itemPicked = true;
      }
   }

   return itemPicked;
}

void RadarProductLayer::UpdateColorTable(
   const std::shared_ptr<MapContext>& mapContext)
{
   logger_->debug("UpdateColorTable()");

   p->colorTableNeedsUpdate_ = false;

   std::shared_ptr<view::RadarProductView> radarProductView =
      mapContext->radar_product_view();
   if (radarProductView == nullptr)
   {
      return;
   }

   const std::vector<boost::gil::rgba8_pixel_t>& colorTable =
      radarProductView->color_table_lut();
   const uint16_t rangeMin = radarProductView->color_table_min();
   const uint16_t rangeMax = radarProductView->color_table_max();

   const auto scale = static_cast<float>(rangeMax - rangeMin);

   p->rgbaColorTable_.resize(colorTable.size() * 4);
   std::memcpy(
      p->rgbaColorTable_.data(), colorTable.data(), p->rgbaColorTable_.size());

   p->rangeMin_               = rangeMin;
   p->scale_                  = scale;
   p->colorTableUploadNeeded_ = true;
}

} // namespace scwx::qt::map
