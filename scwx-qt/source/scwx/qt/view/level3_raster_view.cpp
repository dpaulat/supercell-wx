#include <scwx/qt/view/level3_raster_view.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/common/constants.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/threads.hpp>
#include <scwx/util/time.hpp>
#include <scwx/wsr88d/rpg/raster_data_packet.hpp>

#include <boost/range/irange.hpp>
#include <boost/timer/timer.hpp>

namespace scwx
{
namespace qt
{
namespace view
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
       selectedTime_ {},
       latitude_ {},
       longitude_ {},
       range_ {},
       vcp_ {},
       sweepTime_ {}
   {
   }
   ~Level3RasterViewImpl() = default;

   std::chrono::system_clock::time_point selectedTime_;

   std::vector<float>   vertices_;
   std::vector<uint8_t> dataMoments8_;

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
Level3RasterView::~Level3RasterView() = default;

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

void Level3RasterView::SelectTime(std::chrono::system_clock::time_point time)
{
   p->selectedTime_ = time;
}

void Level3RasterView::ComputeSweep()
{
   logger_->debug("ComputeSweep()");

   boost::timer::cpu_timer timer;

   std::scoped_lock sweepLock(sweep_mutex());

   std::shared_ptr<manager::RadarProductManager> radarProductManager =
      radar_product_manager();

   // Retrieve message from Radar Product Manager
   std::shared_ptr<wsr88d::rpg::Level3Message> message =
      radarProductManager->GetLevel3Data(GetRadarProductName(),
                                         p->selectedTime_);
   if (message == nullptr)
   {
      logger_->debug("Level 3 data not found");
      return;
   }

   // A message with radial data should be a Graphic Product Message
   std::shared_ptr<wsr88d::rpg::GraphicProductMessage> gpm =
      std::dynamic_pointer_cast<wsr88d::rpg::GraphicProductMessage>(message);
   if (gpm == nullptr)
   {
      logger_->warn("Graphic Product Message not found");
      return;
   }
   else if (gpm == graphic_product_message())
   {
      // Skip if this is the message we previously processed
      return;
   }
   set_graphic_product_message(gpm);

   // A message with radial data should have a Product Description Block and
   // Product Symbology Block
   std::shared_ptr<wsr88d::rpg::ProductDescriptionBlock> descriptionBlock =
      message->description_block();
   std::shared_ptr<wsr88d::rpg::ProductSymbologyBlock> symbologyBlock =
      gpm->symbology_block();
   if (descriptionBlock == nullptr || symbologyBlock == nullptr)
   {
      logger_->warn("Missing blocks");
      return;
   }

   // A valid message should have a positive number of layers
   uint16_t numberOfLayers = symbologyBlock->number_of_layers();
   if (numberOfLayers < 1)
   {
      logger_->warn("No layers present in symbology block");
      return;
   }

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
      return;
   }

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
      return;
   }

   p->latitude_  = descriptionBlock->latitude_of_radar();
   p->longitude_ = descriptionBlock->longitude_of_radar();
   p->range_     = descriptionBlock->range();
   p->sweepTime_ =
      scwx::util::TimePoint(descriptionBlock->volume_scan_date(),
                            descriptionBlock->volume_scan_start_time() * 1000);
   p->vcp_ = descriptionBlock->volume_coverage_pattern();

   const GeographicLib::Geodesic& geodesic =
      util::GeographicLib::DefaultGeodesic();

   const uint16_t xResolution = descriptionBlock->x_resolution_raw();
   const uint16_t yResolution = descriptionBlock->y_resolution_raw();
   double         iCoordinate =
      (-rasterData->i_coordinate_start() - 1.0 - p->range_) * 1000.0;
   double jCoordinate =
      (rasterData->j_coordinate_start() + 1.0 + p->range_) * 1000.0;

   size_t numCoordinates =
      static_cast<size_t>(rows + 1) * static_cast<size_t>(maxColumns + 1);
   auto coordinateRange =
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

         const double i = iCoordinate + xResolution * col;
         const double j = jCoordinate - yResolution * row;

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

   for (size_t row = 0; row < rasterData->number_of_rows(); ++row)
   {
      const auto dataMomentsArray8 =
         rasterData->level(static_cast<uint16_t>(row));

      for (size_t bin = 0; bin < dataMomentsArray8.size(); ++bin)
      {
         constexpr size_t vertexCount = 6;

         // Store data moment value
         uint8_t dataValue = dataMomentsArray8[bin];
         if (dataValue < snrThreshold && dataValue != RANGE_FOLDED)
         {
            continue;
         }

         for (size_t m = 0; m < vertexCount; m++)
         {
            dataMoments8[mIndex++] = dataValue;
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

         vertices[vIndex++] = coordinates[offset3];
         vertices[vIndex++] = coordinates[offset3 + 1];

         vertices[vIndex++] = coordinates[offset3];
         vertices[vIndex++] = coordinates[offset3 + 1];

         vertices[vIndex++] = coordinates[offset4];
         vertices[vIndex++] = coordinates[offset4 + 1];

         vertices[vIndex++] = coordinates[offset2];
         vertices[vIndex++] = coordinates[offset2 + 1];
      }
   }
   vertices.resize(vIndex);
   vertices.shrink_to_fit();

   dataMoments8.resize(mIndex);
   dataMoments8.shrink_to_fit();

   timer.stop();
   logger_->debug("Vertices calculated in {}", timer.format(6, "%ws"));

   UpdateColorTable();

   emit SweepComputed();
}

std::shared_ptr<Level3RasterView> Level3RasterView::Create(
   const std::string&                            product,
   std::shared_ptr<manager::RadarProductManager> radarProductManager)
{
   return std::make_shared<Level3RasterView>(product, radarProductManager);
}

} // namespace view
} // namespace qt
} // namespace scwx
