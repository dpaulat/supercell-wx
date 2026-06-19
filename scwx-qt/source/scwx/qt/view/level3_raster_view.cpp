#include <scwx/qt/view/level3_raster_view.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/common/constants.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/threads.hpp>
#include <scwx/util/time.hpp>
#include <scwx/wsr88d/rpg/raster_data_packet.hpp>

#include <boost/range/irange.hpp>
#include <boost/timer/timer.hpp>
#include <units/angle.h>

namespace scwx::qt::view
{

static const std::string logPrefix_ = "scwx::qt::view::level3_raster_view";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr uint16_t RANGE_FOLDED      = 1u;
static constexpr uint32_t VERTICES_PER_BIN  = 6u;
static constexpr uint32_t VALUES_PER_VERTEX = 2u;

class Level3RasterViewImpl
{
public:
   explicit Level3RasterViewImpl() :
       latitude_ {}, longitude_ {}, range_ {}, vcp_ {}, sweepTime_ {}
   {
   }
   ~Level3RasterViewImpl() { threadPool_.join(); };

   [[nodiscard]] inline std::uint8_t
   RemapDataMoment(std::uint8_t dataMoment) const;

   boost::asio::thread_pool threadPool_ {1u};

   std::vector<float>        vertices_ {};
   std::vector<std::uint8_t> dataMoments8_ {};
   std::uint8_t              edgeValue_ {};

   bool showSmoothedRangeFolding_ {false};

   std::shared_ptr<wsr88d::rpg::RasterDataPacket> lastRasterData_ {};
   bool lastShowSmoothedRangeFolding_ {false};
   bool lastSmoothingEnabled_ {false};

   float    latitude_;
   float    longitude_;
   float    range_;
   uint16_t vcp_;

   std::chrono::system_clock::time_point sweepTime_;
};

Level3RasterView::Level3RasterView(
   const std::string&                            product,
   std::shared_ptr<manager::RadarProductManager> radarProductManager) :
    Level3ProductView(product, radarProductManager),
    p(std::make_unique<Level3RasterViewImpl>())
{
}

Level3RasterView::~Level3RasterView()
{
   std::unique_lock sweepLock {sweep_mutex()};
}

boost::asio::thread_pool& Level3RasterView::thread_pool()
{
   return p->threadPool_;
}

float Level3RasterView::range() const
{
   return p->range_;
}

std::chrono::system_clock::time_point Level3RasterView::sweep_time() const
{
   return p->sweepTime_;
}

uint16_t Level3RasterView::vcp() const
{
   return p->vcp_;
}

const std::vector<float>& Level3RasterView::vertices() const
{
   return p->vertices_;
}

std::tuple<const void*, size_t, size_t> Level3RasterView::GetMomentData() const
{
   const void* data;
   size_t      dataSize;
   size_t      componentSize;

   data          = p->dataMoments8_.data();
   dataSize      = p->dataMoments8_.size() * sizeof(uint8_t);
   componentSize = 1;

   return std::tie(data, dataSize, componentSize);
}

void Level3RasterView::ComputeSweep()
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
   types::RadarProductLoadStatus               loadStatus {};
   std::tie(message, foundTime, loadStatus) =
      radarProductManager->GetLevel3Data(GetRadarProductName(), requestedTime);

   set_load_status(loadStatus);

   // If a different time was found than what was requested, update it
   if (requestedTime != foundTime)
   {
      SelectTime(foundTime);
   }

   if (message == nullptr)
   {
      logger_->debug("Level 3 data not found");
      Q_EMIT SweepNotComputed(
         loadStatus == types::RadarProductLoadStatus::ProductNotAvailable ?
            types::NoUpdateReason::NotAvailable :
            types::NoUpdateReason::NotLoaded);
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

   // A message with raster data should have a Raster Data Packet
   std::shared_ptr<wsr88d::rpg::RasterDataPacket> rasterData = nullptr;

   for (uint16_t layer = 0; layer < numberOfLayers; layer++)
   {
      std::vector<std::shared_ptr<wsr88d::rpg::Packet>> packetList =
         symbologyBlock->packet_list(layer);

      for (auto it = packetList.begin(); it != packetList.end(); it++)
      {
         rasterData =
            std::dynamic_pointer_cast<wsr88d::rpg::RasterDataPacket>(*it);

         if (rasterData != nullptr)
         {
            break;
         }
      }

      if (rasterData != nullptr)
      {
         break;
      }
   }

   if (rasterData == nullptr)
   {
      logger_->debug("No raster data found");
      Q_EMIT SweepNotComputed(types::NoUpdateReason::InvalidData);
      return;
   }

   p->lastRasterData_ = rasterData;

   // Calculate raster grid size
   const uint16_t rows       = rasterData->number_of_rows();
   size_t         maxColumns = 0;
   for (uint16_t r = 0; r < rows; r++)
   {
      maxColumns = std::max<size_t>(maxColumns, rasterData->level(r).size());
   }

   if (maxColumns == 0)
   {
      logger_->debug("No raster bins found");
      Q_EMIT SweepNotComputed(types::NoUpdateReason::InvalidData);
      return;
   }

   p->latitude_  = descriptionBlock->latitude_of_radar();
   p->longitude_ = descriptionBlock->longitude_of_radar();
   p->range_     = descriptionBlock->range().value();
   p->sweepTime_ =
      scwx::util::TimePoint(descriptionBlock->volume_scan_date(),
                            descriptionBlock->volume_scan_start_time() * 1000);
   p->vcp_ = descriptionBlock->volume_coverage_pattern();

   const GeographicLib::Geodesic& geodesic =
      util::GeographicLib::DefaultGeodesic();

   const std::uint16_t xResolution = descriptionBlock->x_resolution_raw();
   const std::uint16_t yResolution = descriptionBlock->y_resolution_raw();
   const double        iCoordinate =
      (-rasterData->i_coordinate_start() - 1.0 - p->range_) * 1000.0;
   const double jCoordinate =
      (rasterData->j_coordinate_start() + 1.0 + p->range_) * 1000.0;
   const double xOffset = (smoothingEnabled) ? xResolution * 0.5 : 0.0;
   const double yOffset = (smoothingEnabled) ? yResolution * 0.5 : 0.0;

   const std::size_t numCoordinates =
      static_cast<size_t>(rows + 1) * static_cast<size_t>(maxColumns + 1);
   const auto coordinateRange =
      boost::irange<uint32_t>(0, static_cast<uint32_t>(numCoordinates));

   std::vector<float> coordinates;
   coordinates.resize(numCoordinates * 2);

   // Calculate coordinates
   timer.start();

   std::for_each(
      std::execution::par_unseq,
      coordinateRange.begin(),
      coordinateRange.end(),
      [&](uint32_t index)
      {
         // For each row or column, there is one additional coordinate. Each bin
         // is bounded by 4 coordinates.
         const uint32_t col = index % (rows + 1);
         const uint32_t row = index / (rows + 1);

         const double i = iCoordinate + xResolution * col + xOffset;
         const double j = jCoordinate - yResolution * row - yOffset;

         // Calculate polar coordinates based on i and j
         const double angle  = std::atan2(i, j) * 180.0 / M_PI;
         const double range  = std::sqrt(i * i + j * j);
         const size_t offset = static_cast<size_t>(index) * 2;

         double latitude;
         double longitude;

         geodesic.Direct(
            p->latitude_, p->longitude_, angle, range, latitude, longitude);

         coordinates[offset]     = latitude;
         coordinates[offset + 1] = longitude;
      });

   timer.stop();
   logger_->debug("Coordinates calculated in {}", timer.format(6, "%ws"));

   // Calculate vertices
   timer.start();

   // Setup vertex vector
   std::vector<float>& vertices = p->vertices_;
   size_t              vIndex   = 0;
   vertices.clear();
   vertices.resize(rows * maxColumns * VERTICES_PER_BIN * VALUES_PER_VERTEX);

   // Setup data moment vector
   std::vector<uint8_t>& dataMoments8 = p->dataMoments8_;
   size_t                mIndex       = 0;

   dataMoments8.resize(rows * maxColumns * VERTICES_PER_BIN);

   // Compute threshold at which to display an individual bin
   const uint16_t snrThreshold = descriptionBlock->threshold();

   const std::size_t rowCount = (smoothingEnabled) ?
                                   rasterData->number_of_rows() - 1 :
                                   rasterData->number_of_rows();

   if (smoothingEnabled)
   {
      // For most products other than reflectivity, the edge should not go to
      // the bottom of the color table
      p->edgeValue_ = ComputeEdgeValue();
   }

   for (std::size_t row = 0; row < rowCount; ++row)
   {
      const std::size_t nextRow =
         (row == static_cast<std::size_t>(rasterData->number_of_rows() - 1)) ?
            0 :
            row + 1;

      const auto& dataMomentsArray8 =
         rasterData->level(static_cast<uint16_t>(row));
      const auto& nextDataMomentsArray8 =
         rasterData->level(static_cast<uint16_t>(nextRow));

      for (size_t bin = 0; bin < dataMomentsArray8.size(); ++bin)
      {
         if (!smoothingEnabled)
         {
            static constexpr std::size_t vertexCount = 6;

            // Store data moment value
            const std::uint8_t& dataValue = dataMomentsArray8[bin];
            if (dataValue < snrThreshold && dataValue != RANGE_FOLDED)
            {
               continue;
            }

            for (size_t m = 0; m < vertexCount; m++)
            {
               dataMoments8[mIndex++] = dataValue;
            }
         }
         else
         {
            // Validate indices are all in range
            if (bin + 1 >= dataMomentsArray8.size() ||
                bin + 1 >= nextDataMomentsArray8.size())
            {
               continue;
            }

            const std::uint8_t& dm1 = dataMomentsArray8[bin];
            const std::uint8_t& dm2 = dataMomentsArray8[bin + 1];
            const std::uint8_t& dm3 = nextDataMomentsArray8[bin];
            const std::uint8_t& dm4 = nextDataMomentsArray8[bin + 1];

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

         // Store vertices
         size_t offset1 = (row * (maxColumns + 1) + bin) * 2;
         size_t offset2 = offset1 + 2;
         size_t offset3 = ((row + 1) * (maxColumns + 1) + bin) * 2;
         size_t offset4 = offset3 + 2;

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
Level3RasterViewImpl::RemapDataMoment(std::uint8_t dataMoment) const
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

std::optional<std::uint16_t>
Level3RasterView::GetBinLevel(const common::Coordinate& coordinate) const
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

   std::shared_ptr<wsr88d::rpg::RasterDataPacket> rasterData =
      p->lastRasterData_;
   if (rasterData == nullptr)
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

   units::angle::radians<double> azi1Rads = units::angle::degrees<double>(azi1);

   double j = -std::cos(azi1Rads.value()) * s12;
   double i = std::sin(azi1Rads.value()) * s12;

   const uint32_t xResolution = descriptionBlock->x_resolution_raw();
   const uint32_t yResolution = descriptionBlock->y_resolution_raw();
   double         iStart =
      (-rasterData->i_coordinate_start() - 1.0 - p->range_) * 1000.0;
   double jStart =
      (rasterData->j_coordinate_start() + 1.0 + p->range_) * 1000.0;

   i -= iStart;
   j += jStart;

   std::uint32_t col = static_cast<std::uint32_t>(i / xResolution);
   std::uint32_t row = static_cast<std::uint32_t>(j / yResolution);

   if (row > rasterData->number_of_rows())
   {
      // Coordinate is beyond radar range (latitude)
      return std::nullopt;
   }

   auto& momentData = rasterData->level(static_cast<std::uint16_t>(row));

   if (col > momentData.size())
   {
      // Coordinate is beyond radar range (longitude)
      return std::nullopt;
   }

   return momentData[col];
}

std::shared_ptr<Level3RasterView> Level3RasterView::Create(
   const std::string&                            product,
   std::shared_ptr<manager::RadarProductManager> radarProductManager)
{
   return std::make_shared<Level3RasterView>(product, radarProductManager);
}

} // namespace scwx::qt::view
