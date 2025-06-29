#include <scwx/qt/view/level3_radial_view.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/common/constants.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/threads.hpp>
#include <scwx/util/time.hpp>
#include <scwx/wsr88d/rpg/digital_radial_data_array_packet.hpp>
#include <scwx/wsr88d/rpg/radial_data_packet.hpp>

#include <boost/range/irange.hpp>
#include <boost/timer/timer.hpp>

namespace scwx
{
namespace qt
{
namespace view
{

static const std::string logPrefix_ = "scwx::qt::view::level3_radial_view";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr std::uint32_t kMaxRadialGates_ =
   common::MAX_0_5_DEGREE_RADIALS * common::MAX_DATA_MOMENT_GATES;
static constexpr std::uint32_t kMaxCoordinates_ = kMaxRadialGates_ * 2u;

static constexpr std::uint16_t RANGE_FOLDED      = 1u;
static constexpr std::uint32_t VERTICES_PER_BIN  = 6u;
static constexpr std::uint32_t VALUES_PER_VERTEX = 2u;

class Level3RadialView::Impl
{
public:
   explicit Impl(Level3RadialView* self) : self_ {self}, sweepTime_ {} {}
   ~Impl() { threadPool_.join(); };

   void ComputeCoordinates(
      const std::shared_ptr<wsr88d::rpg::GenericRadialDataPacket>& radialData,
      bool  smoothingEnabled,
      float gateSize);

   [[nodiscard]] inline std::uint8_t
   RemapDataMoment(std::uint8_t dataMoment) const;

   Level3RadialView* self_;

   boost::asio::thread_pool threadPool_ {1u};

   std::vector<float>        coordinates_ {};
   std::vector<float>        vertices_ {};
   std::vector<std::uint8_t> dataMoments8_ {};
   std::uint8_t              edgeValue_ {};

   bool showSmoothedRangeFolding_ {false};

   std::shared_ptr<wsr88d::rpg::GenericRadialDataPacket> lastRadialData_ {};
   bool lastShowSmoothedRangeFolding_ {false};
   bool lastSmoothingEnabled_ {false};

   float                latitude_ {};
   float                longitude_ {};
   std::optional<float> elevation_ {};
   float                range_ {};
   std::uint16_t        vcp_ {};

   std::chrono::system_clock::time_point sweepTime_;
};

Level3RadialView::Level3RadialView(
   const std::string&                            product,
   std::shared_ptr<manager::RadarProductManager> radarProductManager) :
    Level3ProductView(product, radarProductManager),
    p(std::make_unique<Impl>(this))
{
}

Level3RadialView::~Level3RadialView()
{
   std::unique_lock sweepLock {sweep_mutex()};
}

boost::asio::thread_pool& Level3RadialView::thread_pool()
{
   return p->threadPool_;
}

std::optional<float> Level3RadialView::elevation() const
{
   return p->elevation_;
}

float Level3RadialView::range() const
{
   return p->range_;
}

std::chrono::system_clock::time_point Level3RadialView::sweep_time() const
{
   return p->sweepTime_;
}

uint16_t Level3RadialView::vcp() const
{
   return p->vcp_;
}

const std::vector<float>& Level3RadialView::vertices() const
{
   return p->vertices_;
}

std::tuple<const void*, size_t, size_t> Level3RadialView::GetMomentData() const
{
   const void* data;
   size_t      dataSize;
   size_t      componentSize;

   data          = p->dataMoments8_.data();
   dataSize      = p->dataMoments8_.size() * sizeof(uint8_t);
   componentSize = 1;

   return std::tie(data, dataSize, componentSize);
}

void Level3RadialView::ComputeSweep()
{
   logger_->trace("ComputeSweep()");

   boost::timer::cpu_timer timer;

   std::scoped_lock sweepLock(sweep_mutex());

   std::shared_ptr<manager::RadarProductManager> radarProductManager =
      radar_product_manager();
   const bool smoothingEnabled          = smoothing_enabled();
   p->showSmoothedRangeFolding_         = show_smoothed_range_folding();
   const bool& showSmoothedRangeFolding = p->showSmoothedRangeFolding_;

   // Retrieve message from Radar Product Manager
   std::shared_ptr<wsr88d::rpg::Level3Message> message;
   std::chrono::system_clock::time_point       requestedTime {selected_time()};
   std::chrono::system_clock::time_point       foundTime;
   std::tie(message, foundTime) =
      radarProductManager->GetLevel3Data(GetRadarProductName(), requestedTime);

   // If a different time was found than what was requested, update it
   if (requestedTime != foundTime)
   {
      SelectTime(foundTime);
   }

   if (message == nullptr)
   {
      logger_->debug("Level 3 data not found");
      Q_EMIT SweepNotComputed(types::NoUpdateReason::NotLoaded);
      return;
   }

   // A message with radial data should be a Graphic Product Message
   std::shared_ptr<wsr88d::rpg::GraphicProductMessage> gpm =
      std::dynamic_pointer_cast<wsr88d::rpg::GraphicProductMessage>(message);
   if (gpm == nullptr)
   {
      logger_->warn("Graphic Product Message not found");
      Q_EMIT SweepNotComputed(types::NoUpdateReason::InvalidData);
      return;
   }
   else if (gpm == graphic_product_message() &&
            smoothingEnabled == p->lastSmoothingEnabled_ &&
            (showSmoothedRangeFolding == p->lastShowSmoothedRangeFolding_ ||
             !smoothingEnabled))
   {
      // Skip if this is the message we previously processed
      Q_EMIT SweepNotComputed(types::NoUpdateReason::NoChange);
      return;
   }
   set_graphic_product_message(gpm);

   p->lastShowSmoothedRangeFolding_ = showSmoothedRangeFolding;
   p->lastSmoothingEnabled_         = smoothingEnabled;

   // A message with radial data should have a Product Description Block and
   // Product Symbology Block
   std::shared_ptr<wsr88d::rpg::ProductDescriptionBlock> descriptionBlock =
      message->description_block();
   std::shared_ptr<wsr88d::rpg::ProductSymbologyBlock> symbologyBlock =
      gpm->symbology_block();
   if (descriptionBlock == nullptr || symbologyBlock == nullptr)
   {
      logger_->warn("Missing blocks");
      Q_EMIT SweepNotComputed(types::NoUpdateReason::InvalidData);
      return;
   }

   // A valid message should have a positive number of layers
   uint16_t numberOfLayers = symbologyBlock->number_of_layers();
   if (numberOfLayers < 1)
   {
      logger_->warn("No layers present in symbology block");
      Q_EMIT SweepNotComputed(types::NoUpdateReason::InvalidData);
      return;
   }

   logger_->debug("Computing Sweep");

   // A message with radial data should either have a Digital Radial Data
   // Array Packet, or a Radial Data Array Packet
   std::shared_ptr<wsr88d::rpg::DigitalRadialDataArrayPacket>
                                                  digitalDataPacket = nullptr;
   std::shared_ptr<wsr88d::rpg::RadialDataPacket> radialDataPacket  = nullptr;
   std::shared_ptr<wsr88d::rpg::GenericRadialDataPacket> radialData = nullptr;

   for (uint16_t layer = 0; layer < numberOfLayers; layer++)
   {
      std::vector<std::shared_ptr<wsr88d::rpg::Packet>> packetList =
         symbologyBlock->packet_list(layer);

      for (auto it = packetList.begin(); it != packetList.end(); it++)
      {
         // Prefer Digital Radial Data to Radial Data
         digitalDataPacket = std::dynamic_pointer_cast<
            wsr88d::rpg::DigitalRadialDataArrayPacket>(*it);

         if (digitalDataPacket != nullptr)
         {
            break;
         }

         // Otherwise, check for Radial Data
         if (radialDataPacket == nullptr)
         {
            radialDataPacket =
               std::dynamic_pointer_cast<wsr88d::rpg::RadialDataPacket>(*it);
         }
      }

      if (digitalDataPacket != nullptr)
      {
         break;
      }
   }

   if (digitalDataPacket != nullptr)
   {
      radialData = digitalDataPacket;
   }
   else if (radialDataPacket != nullptr)
   {
      radialData = radialDataPacket;
   }
   else
   {
      logger_->debug("No radial data found");
      Q_EMIT SweepNotComputed(types::NoUpdateReason::InvalidData);
      return;
   }

   p->lastRadialData_ = radialData;

   // Valid number of radials is 1-720
   size_t radials = radialData->number_of_radials();
   if (radials < 1 || radials > 720)
   {
      logger_->warn("Unsupported number of radials: {}", radials);
      Q_EMIT SweepNotComputed(types::NoUpdateReason::InvalidData);
      return;
   }

   common::RadialSize radialSize;
   if (radarProductManager->is_tdwr())
   {
      radialSize = common::RadialSize::NonStandard;
   }
   else
   {
      if (radials == common::MAX_0_5_DEGREE_RADIALS)
      {
         radialSize = common::RadialSize::_0_5Degree;
      }
      else if (radials == common::MAX_1_DEGREE_RADIALS)
      {
         radialSize = common::RadialSize::_1Degree;
      }
      else
      {
         radialSize = common::RadialSize::NonStandard;
      }
   }

   const std::vector<float>& coordinates =
      (radialSize == common::RadialSize::NonStandard) ?
         p->coordinates_ :
         radarProductManager->coordinates(radialSize, smoothingEnabled);

   // There should be a positive number of range bins in radial data
   const uint16_t numberOfDataMomentGates = radialData->number_of_range_bins();
   if (numberOfDataMomentGates < 1)
   {
      logger_->warn("No range bins in radial data");
      Q_EMIT SweepNotComputed(types::NoUpdateReason::InvalidData);
      return;
   }

   p->latitude_  = descriptionBlock->latitude_of_radar();
   p->longitude_ = descriptionBlock->longitude_of_radar();
   p->range_     = descriptionBlock->range().value();
   p->elevation_ =
      descriptionBlock->has_elevation() ?
         static_cast<float>(descriptionBlock->elevation().value()) :
         std::optional<float> {};
   p->sweepTime_ =
      scwx::util::TimePoint(descriptionBlock->volume_scan_date(),
                            descriptionBlock->volume_scan_start_time() * 1000);
   p->vcp_ = descriptionBlock->volume_coverage_pattern();

   // Calculate vertices
   timer.start();

   // Setup vertex vector
   std::vector<float>& vertices = p->vertices_;
   size_t              vIndex   = 0;
   vertices.clear();
   vertices.resize(radials * numberOfDataMomentGates * VERTICES_PER_BIN *
                   VALUES_PER_VERTEX);

   // Setup data moment vector
   std::vector<uint8_t>& dataMoments8 = p->dataMoments8_;
   size_t                mIndex       = 0;

   dataMoments8.resize(radials * numberOfDataMomentGates * VERTICES_PER_BIN);

   // Compute threshold at which to display an individual bin
   const uint16_t snrThreshold = descriptionBlock->threshold();

   // Compute gate interval
   const std::uint16_t dataMomentInterval =
      descriptionBlock->x_resolution_raw();

   // Get the gate length in meters. Use dataMomentInterval for NonStandard to
   // avoid generating >1 base gates per bin.
   const float gateLength = radialSize == common::RadialSize::NonStandard ?
                               static_cast<float>(dataMomentInterval) :
                               radarProductManager->gate_size();

   // Determine which radial to start at
   std::uint16_t startRadial;
   if (radialSize == common::RadialSize::NonStandard)
   {
      p->ComputeCoordinates(radialData, smoothingEnabled, gateLength);
      startRadial = 0;
   }
   else
   {
      const float radialMultiplier = radials / 360.0f;
      const float startAngle       = radialData->start_angle(0);
      startRadial = std::lroundf(startAngle * radialMultiplier);
   }

   // Compute gate size (number of base gates per bin)
   const std::uint16_t gateSize = std::max<std::uint16_t>(
      1, dataMomentInterval / static_cast<std::uint16_t>(gateLength));

   // Compute gate range [startGate, endGate)
   std::uint16_t       startGate = 0;
   const std::uint16_t endGate =
      std::min<std::uint16_t>(startGate + numberOfDataMomentGates * gateSize,
                              common::MAX_DATA_MOMENT_GATES);

   if (smoothingEnabled)
   {
      // If smoothing is enabled, the start gate is incremented by one, as we
      // are skipping the radar site origin. The end gate is unaffected, as
      // we need to draw one less data point.
      ++startGate;

      // For most products other than reflectivity, the edge should not go to
      // the bottom of the color table
      p->edgeValue_ = ComputeEdgeValue();
   }

   for (std::uint16_t radial = 0; radial < radialData->number_of_radials();
        ++radial)
   {
      const auto& dataMomentsArray8 = radialData->level(radial);

      const std::uint16_t nextRadial =
         (radial == radialData->number_of_radials() - 1) ? 0 : radial + 1;
      const auto& nextDataMomentsArray8 = radialData->level(nextRadial);

      for (std::uint16_t gate = startGate, i = 0; gate + gateSize <= endGate;
           gate += gateSize, ++i)
      {
         size_t vertexCount = (gate > 0) ? 6 : 3;

         if (!smoothingEnabled)
         {
            // Store data moment value
            const uint8_t dataValue =
               (i < dataMomentsArray8.size()) ? dataMomentsArray8[i] : 0;
            if (dataValue < snrThreshold && dataValue != RANGE_FOLDED)
            {
               continue;
            }

            for (size_t m = 0; m < vertexCount; m++)
            {
               dataMoments8[mIndex++] = dataValue;
            }
         }
         else if (gate > 0)
         {
            // Validate indices are all in range
            if (i + 1 >= numberOfDataMomentGates)
            {
               continue;
            }

            const std::uint8_t& dm1 = dataMomentsArray8[i];
            const std::uint8_t& dm2 = dataMomentsArray8[i + 1];
            const std::uint8_t& dm3 = nextDataMomentsArray8[i];
            const std::uint8_t& dm4 = nextDataMomentsArray8[i + 1];

            if ((!showSmoothedRangeFolding && //
                 (dm1 < snrThreshold || dm1 == RANGE_FOLDED) &&
                 (dm2 < snrThreshold || dm2 == RANGE_FOLDED) &&
                 (dm3 < snrThreshold || dm3 == RANGE_FOLDED) &&
                 (dm4 < snrThreshold || dm4 == RANGE_FOLDED)) ||
                (showSmoothedRangeFolding && //
                 dm1 < snrThreshold && dm1 != RANGE_FOLDED &&
                 dm2 < snrThreshold && dm2 != RANGE_FOLDED &&
                 dm3 < snrThreshold && dm3 != RANGE_FOLDED &&
                 dm4 < snrThreshold && dm4 != RANGE_FOLDED))
            {
               // Skip only if all data moments are hidden
               continue;
            }

            // The order must match the store vertices section below
            dataMoments8[mIndex++] = p->RemapDataMoment(dm1);
            dataMoments8[mIndex++] = p->RemapDataMoment(dm2);
            dataMoments8[mIndex++] = p->RemapDataMoment(dm4);
            dataMoments8[mIndex++] = p->RemapDataMoment(dm1);
            dataMoments8[mIndex++] = p->RemapDataMoment(dm3);
            dataMoments8[mIndex++] = p->RemapDataMoment(dm4);
         }
         else
         {
            // If smoothing is enabled, gate should never start at zero
            // (radar site origin)
            logger_->error("Smoothing enabled, gate should not start at zero");
            continue;
         }

         // Store vertices
         if (gate > 0)
         {
            const uint16_t baseCoord = gate - 1;

            size_t offset1 = ((startRadial + radial) % radials *
                                 common::MAX_DATA_MOMENT_GATES +
                              baseCoord) *
                             2;
            size_t offset2 = offset1 + gateSize * 2;
            size_t offset3 = (((startRadial + radial + 1) % radials) *
                                 common::MAX_DATA_MOMENT_GATES +
                              baseCoord) *
                             2;
            size_t offset4 = offset3 + gateSize * 2;

            vertices[vIndex++] = coordinates[offset1];
            vertices[vIndex++] = coordinates[offset1 + 1];

            vertices[vIndex++] = coordinates[offset2];
            vertices[vIndex++] = coordinates[offset2 + 1];

            vertices[vIndex++] = coordinates[offset4];
            vertices[vIndex++] = coordinates[offset4 + 1];

            vertices[vIndex++] = coordinates[offset1];
            vertices[vIndex++] = coordinates[offset1 + 1];

            vertices[vIndex++] = coordinates[offset3];
            vertices[vIndex++] = coordinates[offset3 + 1];

            vertices[vIndex++] = coordinates[offset4];
            vertices[vIndex++] = coordinates[offset4 + 1];
         }
         else
         {
            const uint16_t baseCoord = gate;

            size_t offset1 = ((startRadial + radial) % radials *
                                 common::MAX_DATA_MOMENT_GATES +
                              baseCoord) *
                             2;
            size_t offset2 = (((startRadial + radial + 1) % radials) *
                                 common::MAX_DATA_MOMENT_GATES +
                              baseCoord) *
                             2;

            vertices[vIndex++] = p->latitude_;
            vertices[vIndex++] = p->longitude_;

            vertices[vIndex++] = coordinates[offset1];
            vertices[vIndex++] = coordinates[offset1 + 1];

            vertices[vIndex++] = coordinates[offset2];
            vertices[vIndex++] = coordinates[offset2 + 1];
         }
      }
   }
   vertices.resize(vIndex);
   vertices.shrink_to_fit();

   dataMoments8.resize(mIndex);
   dataMoments8.shrink_to_fit();

   timer.stop();
   logger_->debug("Vertices calculated in {}", timer.format(6, "%ws"));

   UpdateColorTableLut();

   Q_EMIT SweepComputed();
}

std::uint8_t
Level3RadialView::Impl::RemapDataMoment(std::uint8_t dataMoment) const
{
   if (dataMoment != 0 &&
       (dataMoment != RANGE_FOLDED || showSmoothedRangeFolding_))
   {
      return dataMoment;
   }
   else
   {
      return edgeValue_;
   }
}

void Level3RadialView::Impl::ComputeCoordinates(
   const std::shared_ptr<wsr88d::rpg::GenericRadialDataPacket>& radialData,
   bool  smoothingEnabled,
   float gateSize)
{
   logger_->debug("ComputeCoordinates()");

   boost::timer::cpu_timer timer;

   const GeographicLib::Geodesic& geodesic(
      util::GeographicLib::DefaultGeodesic());

   auto         radarProductManager = self_->radar_product_manager();
   auto         radarSite           = radarProductManager->radar_site();
   const double radarLatitude       = radarSite->latitude();
   const double radarLongitude      = radarSite->longitude();

   // Calculate azimuth coordinates
   timer.start();

   coordinates_.resize(kMaxCoordinates_);

   const std::uint16_t numRadials   = radialData->number_of_radials();
   const std::uint16_t numRangeBins = radialData->number_of_range_bins();

   auto radials = boost::irange<std::uint32_t>(0u, numRadials);
   auto gates   = boost::irange<std::uint32_t>(0u, numRangeBins);

   const float gateRangeOffset = (smoothingEnabled) ?
                                    // Center of the first gate is half the gate
                                    // size distance from the radar site
                                    0.5f :
                                    // Far end of the first gate is the gate
                                    // size distance from the radar site
                                    1.0f;

   std::for_each(
      std::execution::par_unseq,
      radials.begin(),
      radials.end(),
      [&](std::uint32_t radial)
      {
         float angle = radialData->start_angle(radial);

         if (smoothingEnabled)
         {
            static constexpr float kDeltaAngleFactor = 0.5f;
            angle += radialData->delta_angle(radial) * kDeltaAngleFactor;
         }

         std::for_each(
            std::execution::par_unseq,
            gates.begin(),
            gates.end(),
            [&](std::uint32_t gate)
            {
               const std::uint32_t radialGate =
                  radial * common::MAX_DATA_MOMENT_GATES + gate;
               const float range =
                  (static_cast<float>(gate) + gateRangeOffset) * gateSize;
               const std::size_t offset = static_cast<size_t>(radialGate) * 2;
               if (offset + 1 >= coordinates_.size())
               {
                  return;
               }

               double latitude  = 0.0;
               double longitude = 0.0;

               geodesic.Direct(radarLatitude,
                               radarLongitude,
                               angle,
                               range,
                               latitude,
                               longitude);

               coordinates_[offset]     = static_cast<float>(latitude);
               coordinates_[offset + 1] = static_cast<float>(longitude);
            });
      });
   timer.stop();
   logger_->debug("Coordinates calculated in {}", timer.format(6, "%ws"));
}

std::optional<std::uint16_t>
Level3RadialView::GetBinLevel(const common::Coordinate& coordinate) const
{
   auto gpm = graphic_product_message();
   if (gpm == nullptr)
   {
      return std::nullopt;
   }

   std::shared_ptr<wsr88d::rpg::ProductDescriptionBlock> descriptionBlock =
      gpm->description_block();
   if (descriptionBlock == nullptr)
   {
      return std::nullopt;
   }

   std::shared_ptr<wsr88d::rpg::GenericRadialDataPacket> radialData =
      p->lastRadialData_;
   if (radialData == nullptr)
   {
      return std::nullopt;
   }

   auto         radarProductManager = radar_product_manager();
   auto         radarSite           = radarProductManager->radar_site();
   const double radarLatitude       = radarSite->latitude();
   const double radarLongitude      = radarSite->longitude();

   // Determine distance and azimuth of coordinate relative to radar location
   double s12;  // Distance (meters)
   double azi1; // Azimuth (degrees)
   double azi2; // Unused
   util::GeographicLib::DefaultGeodesic().Inverse(radarLatitude,
                                                  radarLongitude,
                                                  coordinate.latitude_,
                                                  coordinate.longitude_,
                                                  s12,
                                                  azi1,
                                                  azi2);

   if (std::isnan(azi1))
   {
      // If a problem occurred with the geodesic inverse calculation
      return std::nullopt;
   }

   // Azimuth is returned as [-180, 180) from the geodesic inverse, we need a
   // range of [0, 360)
   while (azi1 < 0.0)
   {
      azi1 += 360.0;
   }

   // Compute gate interval
   const std::uint16_t gates = radialData->number_of_range_bins();
   const std::uint16_t dataMomentInterval =
      descriptionBlock->x_resolution_raw();
   std::uint16_t gate = s12 / dataMomentInterval;

   if (gate >= gates)
   {
      // Coordinate is beyond radar range
      return std::nullopt;
   }

   // Find Radial
   const std::uint16_t numRadials = radialData->number_of_radials();
   auto                radials = boost::irange<std::uint32_t>(0u, numRadials);

   auto radial = std::find_if( //
      std::execution::par_unseq,
      radials.begin(),
      radials.end(),
      [&](std::uint32_t i)
      {
         bool   found      = false;
         double startAngle = radialData->start_angle(i);
         double nextAngle  = radialData->start_angle((i + 1) % numRadials);

         if (startAngle < nextAngle)
         {
            if (startAngle <= azi1 && azi1 < nextAngle)
            {
               found = true;
            }
         }
         else
         {
            // If the bin crosses 0/360 degrees, special handling is needed
            if (startAngle <= azi1 || azi1 < nextAngle)
            {
               found = true;
            }
         }

         return found;
      });

   if (radial == radials.end())
   {
      // No radial was found (not likely to happen without a gap in data)
      return std::nullopt;
   }

   // Compute threshold at which to display an individual bin
   const std::uint16_t snrThreshold = descriptionBlock->threshold();
   const std::uint8_t  level        = radialData->level(*radial).at(gate);

   if (level < snrThreshold && level != RANGE_FOLDED)
   {
      return std::nullopt;
   }

   return level;
}

std::shared_ptr<Level3RadialView> Level3RadialView::Create(
   const std::string&                            product,
   std::shared_ptr<manager::RadarProductManager> radarProductManager)
{
   return std::make_shared<Level3RadialView>(product, radarProductManager);
}

} // namespace view
} // namespace qt
} // namespace scwx
