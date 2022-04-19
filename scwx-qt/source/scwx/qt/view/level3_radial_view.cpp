#include <scwx/qt/view/level3_radial_view.hpp>
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

static constexpr uint16_t RANGE_FOLDED      = 1u;
static constexpr uint32_t VERTICES_PER_BIN  = 6u;
static constexpr uint32_t VALUES_PER_VERTEX = 2u;

class Level3RadialViewImpl
{
public:
   explicit Level3RadialViewImpl(
      const std::string&                            product,
      std::shared_ptr<manager::RadarProductManager> radarProductManager) :
       product_ {product},
       radarProductManager_ {radarProductManager},
       selectedTime_ {},
       latitude_ {},
       longitude_ {},
       range_ {},
       vcp_ {},
       sweepTime_ {}
   {
   }
   ~Level3RadialViewImpl() = default;

   std::string                                   product_;
   std::shared_ptr<manager::RadarProductManager> radarProductManager_;

   std::chrono::system_clock::time_point selectedTime_;

   std::vector<float>   vertices_;
   std::vector<uint8_t> dataMoments8_;

   float    latitude_;
   float    longitude_;
   float    range_;
   uint16_t vcp_;

   std::chrono::system_clock::time_point sweepTime_;
};

Level3RadialView::Level3RadialView(
   const std::string&                            product,
   std::shared_ptr<manager::RadarProductManager> radarProductManager) :
    Level3ProductView(product),
    p(std::make_unique<Level3RadialViewImpl>(product, radarProductManager))
{
}
Level3RadialView::~Level3RadialView() = default;

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

void Level3RadialView::SelectTime(std::chrono::system_clock::time_point time)
{
   p->selectedTime_ = time;
}

void Level3RadialView::ComputeSweep()
{
   logger_->debug("ComputeSweep()");

   boost::timer::cpu_timer timer;

   std::scoped_lock sweepLock(sweep_mutex());

   // Retrieve message from Radar Product Manager
   std::shared_ptr<wsr88d::rpg::Level3Message> message =
      p->radarProductManager_->GetLevel3Data(p->product_, p->selectedTime_);

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

   // A message with radial data should either have a Digital Radial Data Array
   // Packet, or a Radial Data Array Packet (TODO)
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
      return;
   }

   // Assume the number of radials should be 360 or 720
   const size_t radials = radialData->number_of_radials();
   if (radials != 360 && radials != 720)
   {
      logger_->warn("Unsupported number of radials: {}", radials);
      return;
   }

   const common::RadialSize radialSize =
      (radials == common::MAX_0_5_DEGREE_RADIALS) ?
         common::RadialSize::_0_5Degree :
         common::RadialSize::_1Degree;
   const std::vector<float>& coordinates =
      p->radarProductManager_->coordinates(radialSize);

   // There should be a positive number of range bins in radial data
   const uint16_t gates = radialData->number_of_range_bins();
   if (gates < 1)
   {
      logger_->warn("No range bins in radial data");
      return;
   }

   p->latitude_  = descriptionBlock->latitude_of_radar();
   p->longitude_ = descriptionBlock->longitude_of_radar();
   p->range_     = descriptionBlock->range();
   p->sweepTime_ =
      util::TimePoint(descriptionBlock->volume_scan_date(),
                      descriptionBlock->volume_scan_start_time() * 1000);
   p->vcp_ = descriptionBlock->volume_coverage_pattern();

   // Calculate vertices
   timer.start();

   // Setup vertex vector
   std::vector<float>& vertices = p->vertices_;
   size_t              vIndex   = 0;
   vertices.clear();
   vertices.resize(radials * gates * VERTICES_PER_BIN * VALUES_PER_VERTEX);

   // Setup data moment vector
   std::vector<uint8_t>& dataMoments8 = p->dataMoments8_;
   size_t                mIndex       = 0;

   dataMoments8.resize(radials * gates * VERTICES_PER_BIN);

   // Compute threshold at which to display an individual bin
   const float    scale        = descriptionBlock->scale();
   const float    offset       = descriptionBlock->offset();
   const uint16_t snrThreshold = descriptionBlock->threshold();

   // Determine which radial to start at
   const float    radialMultiplier = radials / 360.0f;
   const float    startAngle       = radialData->start_angle(0);
   const uint16_t startRadial = std::lroundf(startAngle * radialMultiplier);

   for (uint16_t radial = 0; radial < radialData->number_of_radials(); radial++)
   {
      const auto dataMomentsArray8 = radialData->level(radial);

      // Compute gate interval
      const uint16_t dataMomentInterval  = descriptionBlock->x_resolution_raw();
      const uint16_t dataMomentIntervalH = dataMomentInterval / 2;
      const uint16_t dataMomentRange     = dataMomentIntervalH;

      // Compute gate size (number of base gates per bin)
      const uint16_t gateSize = std::max<uint16_t>(
         1,
         dataMomentInterval /
            static_cast<uint16_t>(p->radarProductManager_->gate_size()));

      // Compute gate range [startGate, endGate)
      const uint16_t startGate = 0;
      const uint16_t endGate   = std::min<uint16_t>(
         startGate + gates * gateSize, common::MAX_DATA_MOMENT_GATES);

      for (uint16_t gate = startGate, i = 0; gate + gateSize <= endGate;
           gate += gateSize, ++i)
      {
         size_t vertexCount = (gate > 0) ? 6 : 3;

         // Store data moment value
         uint8_t dataValue =
            (i < dataMomentsArray8.size()) ? dataMomentsArray8[i] : 0;
         if (dataValue < snrThreshold && dataValue != RANGE_FOLDED)
         {
            continue;
         }

         for (size_t m = 0; m < vertexCount; m++)
         {
            dataMoments8[mIndex++] = dataValue;
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

            vertices[vIndex++] = coordinates[offset3];
            vertices[vIndex++] = coordinates[offset3 + 1];

            vertices[vIndex++] = coordinates[offset3];
            vertices[vIndex++] = coordinates[offset3 + 1];

            vertices[vIndex++] = coordinates[offset4];
            vertices[vIndex++] = coordinates[offset4 + 1];

            vertices[vIndex++] = coordinates[offset2];
            vertices[vIndex++] = coordinates[offset2 + 1];

            vertexCount = 6;
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

            vertexCount = 3;
         }
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

std::shared_ptr<Level3RadialView> Level3RadialView::Create(
   const std::string&                            product,
   std::shared_ptr<manager::RadarProductManager> radarProductManager)
{
   return std::make_shared<Level3RadialView>(product, radarProductManager);
}

} // namespace view
} // namespace qt
} // namespace scwx
