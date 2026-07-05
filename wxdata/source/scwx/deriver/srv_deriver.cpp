#include <scwx/deriver/srv_deriver.hpp>

#include <scwx/deriver/data/derived_radial_data.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>
#include <scwx/wsr88d/rpg/graphic_product_message.hpp>
#include <scwx/wsr88d/rpg/digital_radial_data_array_packet.hpp>
#include <scwx/wsr88d/rpg/radial_data_packet.hpp>

namespace scwx::deriver
{

static const std::string logPrefix_ = "scwx::deriver::srv_deriver";
static const auto        logger_    = util::Logger::Create(logPrefix_);

class SrvDeriver::Impl
{
public:
   explicit Impl() = default;
};

static const float kDegreesToRadians_ =
   units::angle::radians<float>(units::angle::degrees<float>(1)).value();

SrvDeriver::SrvDeriver() : p {std::make_unique<Impl>()} {}
SrvDeriver::~SrvDeriver() = default;

std::shared_ptr<data::DerivedData>
SrvDeriver::GetOutput(const std::string& product)
{
   // Find which products we need for this elevation
   const auto& derivedProductIt = deriveable_products().find(product);
   if (derivedProductIt == deriveable_products().cend())
   {
      return nullptr;
   }
   const auto& derivedProduct = derivedProductIt->second;

   const std::shared_ptr<wsr88d::rpg::Level3Message> srmFile =
      GetLevel3Input(derivedProduct.level3AwipsIds_[1]);
   const std::shared_ptr<wsr88d::rpg::Level3Message> sdvFile =
      GetLevel3Input(derivedProduct.level3AwipsIds_[0]);
   if (srmFile == nullptr || sdvFile == nullptr)
   {
      return nullptr;
   }

   const auto srmMessage =
      std::dynamic_pointer_cast<wsr88d::rpg::GraphicProductMessage>(srmFile);
   const auto sdvMessage =
      std::dynamic_pointer_cast<wsr88d::rpg::GraphicProductMessage>(sdvFile);
   if (srmMessage == nullptr || sdvMessage == nullptr)
   {
      return nullptr;
   }

   const auto srmDescriptionBlock = srmMessage->description_block();
   const auto sdvDescriptionBlock = sdvMessage->description_block();
   const auto sdvSymbologyBlock   = sdvMessage->symbology_block();
   // A message with radial data should have a Product Description Block and
   // Product Symbology Block
   if (srmDescriptionBlock == nullptr || sdvDescriptionBlock == nullptr ||
       sdvSymbologyBlock == nullptr)
   {
      return nullptr;
   }

   // A valid message should have a positive number of layers
   const uint16_t sdvNumberOfLayers = sdvSymbologyBlock->number_of_layers();
   if (sdvNumberOfLayers < 1)
   {
      return nullptr;
   }

   // Get average storm speed and direction
   const float meanStormSpeed = units::velocity::meters_per_second<float>(
                                   srmDescriptionBlock->avg_storm_speed())
                                   .value();
   const float meanStormDirection =
      srmDescriptionBlock->avg_storm_dir().value();

   // A message with radial data should either have a Digital Radial Data
   // Array Packet, or a Radial Data Array Packet
   std::shared_ptr<wsr88d::rpg::DigitalRadialDataArrayPacket>
                                                  digitalDataPacket = nullptr;
   std::shared_ptr<wsr88d::rpg::RadialDataPacket> radialDataPacket  = nullptr;
   std::shared_ptr<wsr88d::rpg::GenericRadialDataPacket> radialData = nullptr;
   for (uint16_t layer = 0; layer < sdvNumberOfLayers; layer++)
   {
      const std::vector<std::shared_ptr<wsr88d::rpg::Packet>> packetList =
         sdvSymbologyBlock->packet_list(layer);

      for (auto& it : packetList)
      {
         // Prefer Digital Radial Data to Radial Data
         digitalDataPacket = std::dynamic_pointer_cast<
            wsr88d::rpg::DigitalRadialDataArrayPacket>(it);

         if (digitalDataPacket != nullptr)
         {
            break;
         }

         // Otherwise, check for Radial Data
         if (radialDataPacket == nullptr)
         {
            radialDataPacket =
               std::dynamic_pointer_cast<wsr88d::rpg::RadialDataPacket>(it);
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
      return nullptr;
   }

   // Valid number of radials is 1-720
   const std::uint16_t radials = radialData->number_of_radials();
   const std::uint16_t gates   = radialData->number_of_range_bins();

   const float dataOffset = sdvDescriptionBlock->offset();
   const float dataScale  = sdvDescriptionBlock->scale();

   // Find the range of values that need to be covered
   float maxSRVValue = -std::numeric_limits<float>::infinity();
   float minSRVValue = std::numeric_limits<float>::infinity();
   for (std::uint16_t radial = 0; radial < radials; ++radial)
   {
      const auto& dataMomentsArray8 = radialData->level(radial);
      const float startAngle        = radialData->start_angle(radial);
      const float deltaAngle        = radialData->delta_angle(radial);

      const float stormRadialVelocity =
         -meanStormSpeed *
         std::cos((meanStormDirection - startAngle + deltaAngle / 2) *
                  kDegreesToRadians_);

      uint8_t maxLevel = std::numeric_limits<uint8_t>::min();
      uint8_t minLevel = std::numeric_limits<uint8_t>::max();
      for (std::uint16_t gate = 0; gate < gates; ++gate)
      {
         const uint8_t dataLevel =
            (gate < dataMomentsArray8.size()) ? dataMomentsArray8[gate] : 0;

         // Inlined form product_description_block for speed.
         if (dataLevel < 2)
         {
            continue;
         }
         maxLevel = std::max(maxLevel, dataLevel);
         minLevel = std::min(minLevel, dataLevel);
      }

      const float maxValue = static_cast<float>(maxLevel) * dataScale +
                             dataOffset - stormRadialVelocity;
      const float minValue = static_cast<float>(minLevel) * dataScale +
                             dataOffset - stormRadialVelocity;
      maxSRVValue = std::max(maxSRVValue, maxValue);
      minSRVValue = std::min(minSRVValue, minValue);
   }

   if (maxSRVValue <= minSRVValue)
   {
      // This could mean there is no bins, or be an error.
      return nullptr;
   }

   const float outputScale = 254 / (maxSRVValue - minSRVValue);

   auto output = std::make_shared<data::DerivedRadialData>(radials, gates);
   for (std::uint16_t radial = 0; radial < radials; ++radial)
   {
      const auto& dataMomentsArray8 = radialData->level(radial);
      const float startAngle        = radialData->start_angle(radial);
      const float deltaAngle        = radialData->delta_angle(radial);

      output->SetRadial(radial, startAngle, deltaAngle);

      // TODO should this angle be center or edge?
      const float stormRadialVelocity =
         -meanStormSpeed *
         std::cos((meanStormDirection - startAngle + deltaAngle / 2) *
                  kDegreesToRadians_);

      auto& outputRadialMoments = output->levels(radial);

      for (std::uint16_t gate = 0; gate < gates; ++gate)
      {
         const uint8_t dataLevel =
            (gate < dataMomentsArray8.size()) ? dataMomentsArray8[gate] : 0;

         // Inlined form product_description_block for speed.
         if (dataLevel < 2)
         {
            outputRadialMoments[gate] = dataLevel;
         }
         else
         {
            const float velocity =
               static_cast<float>(dataLevel) * dataScale + dataOffset;
            // Storm Relative Mean Radial Velocity = (Mean Radial Velocity) -
            // StrmSpd * cos(StrmDir - CurrAzm)
            const float outputValue = velocity - stormRadialVelocity;
            const auto  outputLevel = static_cast<uint8_t>(
               (outputValue - minSRVValue) * outputScale + 2);
            outputRadialMoments[gate] = outputLevel;
         }
      }
   }

   output->meta_data().hasElevation       = true;
   output->meta_data().elevation          = sdvDescriptionBlock->elevation();
   output->meta_data().scale              = 1 / outputScale;
   output->meta_data().offset             = minSRVValue;
   output->meta_data().range              = sdvDescriptionBlock->range();
   output->meta_data().dataMomentInterval = sdvDescriptionBlock->x_resolution();
   output->meta_data().sweepTime          = scwx::util::TimePoint(
      sdvDescriptionBlock->volume_scan_date(),
      // NOLINTNEXTLINE seconds to ms
      sdvDescriptionBlock->volume_scan_start_time() * 1000);
   output->meta_data().vcp = sdvDescriptionBlock->volume_coverage_pattern();
   output->meta_data().threshold = 2;
   // NOLINTNEXTLINE This is a 256 level
   output->meta_data().numberOfLevels = 256;

   return output;
}

const std::unordered_map<std::string, DerivedProductInfo>&
SrvDeriver::deriveable_products()
{
   const static std::unordered_map<std::string, DerivedProductInfo>
      derivableProducts_ = {
         {"SRV-AVG-X", {"SRV-AVG-X", {"NXG", "N0S"}, {}}},
         {"SRV-AVG-Y", {"SRV-AVG-Y", {"NYG", "N0S"}, {}}},
         {"SRV-AVG-Z", {"SRV-AVG-Z", {"NZG", "N0S"}, {}}},
         {"SRV-AVG-0", {"SRV-AVG-0", {"N0G", "N0S"}, {}}},
         {"SRV-AVG-A", {"SRV-AVG-A", {"NAG", "N0S"}, {}}},
         {"SRV-AVG-1", {"SRV-AVG-1", {"N1G", "N0S"}, {}}},
         {"SRV-AVG-B", {"SRV-AVG-B", {"NBG", "N0S"}, {}}},
         {"SRV-AVG-2", {"SRV-AVG-2", {"N2G", "N0S"}, {}}},
         {"SRV-AVG-3", {"SRV-AVG-3", {"N3G", "N0S"}, {}}},
      };
   return derivableProducts_;
}

} // namespace scwx::deriver
