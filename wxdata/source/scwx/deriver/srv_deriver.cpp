#include <scwx/deriver/srv_deriver.hpp>

#include <scwx/deriver/data/derived_radial_data.hpp>
#include <scwx/wsr88d/rpg/graphic_product_message.hpp>
#include <scwx/wsr88d/rpg/digital_radial_data_array_packet.hpp>
#include <scwx/wsr88d/rpg/radial_data_packet.hpp>

namespace scwx::deriver
{

static const std::unordered_set<std::string> kLevel3InputProducts_ = {"SRM",
                                                                      "SDV"};

class SrvDeriver::Impl
{
public:
   explicit Impl() = default;
   std::shared_ptr<data::DerivedRadialData> output_ {nullptr};
};

SrvDeriver::SrvDeriver() : p {std::make_unique<Impl>()} {}
SrvDeriver::~SrvDeriver() = default;

const std::unordered_set<std::string>& SrvDeriver::GetLevel3InputProducts()
{
   return kLevel3InputProducts_;
}

bool SrvDeriver::NeedsLevel2Input()
{
   return false;
}

std::shared_ptr<data::DerivedData> SrvDeriver::GetOutput()
{
   if (!GetChanged())
   {
      return p->output_;
   }
   SetChanged(false);

   std::shared_ptr<wsr88d::Level3File> srmFile = GetLevel3File("SRM");
   std::shared_ptr<wsr88d::Level3File> sdvFile = GetLevel3File("SDV");
   if (srmFile == nullptr || sdvFile == nullptr)
   {
      return p->output_;
   }

   auto srmMessage =
      std::dynamic_pointer_cast<wsr88d::rpg::GraphicProductMessage>(
         srmFile->message());
   auto sdvMessage =
      std::dynamic_pointer_cast<wsr88d::rpg::GraphicProductMessage>(
         sdvFile->message());
   if (srmMessage == nullptr || sdvMessage == nullptr)
   {
      return p->output_;
   }

   auto srmDescriptionBlock = srmMessage->description_block();
   auto sdvDescriptionBlock = sdvMessage->description_block();
   auto sdvSymbologyBlock   = sdvMessage->symbology_block();
   // A message with radial data should have a Product Description Block and
   // Product Symbology Block
   if (srmDescriptionBlock == nullptr || sdvDescriptionBlock == nullptr ||
       sdvSymbologyBlock == nullptr)
   {
      return p->output_;
   }

   // A valid message should have a positive number of layers
   uint16_t sdvNumberOfLayers = sdvSymbologyBlock->number_of_layers();
   if (sdvNumberOfLayers < 1)
   {
      return p->output_;
   }

   // Get average storm speed and direction
   float meanStormSpeed =
      static_cast<float>(srmDescriptionBlock->parameter(8)) * 0.1f;
   float meanStormDirection =
      static_cast<float>(srmDescriptionBlock->parameter(9)) * 0.1f;

   // A message with radial data should either have a Digital Radial Data
   // Array Packet, or a Radial Data Array Packet
   std::shared_ptr<wsr88d::rpg::DigitalRadialDataArrayPacket>
                                                  digitalDataPacket = nullptr;
   std::shared_ptr<wsr88d::rpg::RadialDataPacket> radialDataPacket  = nullptr;
   std::shared_ptr<wsr88d::rpg::GenericRadialDataPacket> radialData = nullptr;
   for (uint16_t layer = 0; layer < sdvNumberOfLayers; layer++)
   {
      std::vector<std::shared_ptr<wsr88d::rpg::Packet>> packetList =
         sdvSymbologyBlock->packet_list(layer);

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
      return p->output_;
   }

   // Valid number of radials is 1-720
   const std::uint16_t radials = radialData->number_of_radials();
   const std::uint16_t gates   = radialData->number_of_range_bins();
   if (radials < 1 || radials > 720)
   {
      return p->output_;
   }
   // From this point on, it cannot fail
   p->output_ = std::make_shared<data::DerivedRadialData>(radials, gates);

   const float dataOffset = sdvDescriptionBlock->offset();
   const float dataScale  = sdvDescriptionBlock->scale();

   for (std::uint16_t radial = 0; radial < radials; ++radial)
   {
      const auto& dataMomentsArray8 = radialData->level(radial);
      const float startAngle        = radialData->start_angle(radial);
      const float deltaAngle        = radialData->delta_angle(radial);

      p->output_->SetRadial(radial, startAngle, deltaAngle);

      for (std::uint16_t gate = 0; gate < gates; ++gate)
      {
         const uint8_t dataLevel =
            (gate < dataMomentsArray8.size()) ? dataMomentsArray8[gate] : 0;

         // Inlined form product_description_block for speed.
         if (dataLevel == 0)
         {
            p->output_->SetBin(
               radial, gate, 0, wsr88d::DataLevelCode::BelowThreshold);
         }
         else if (dataLevel == 1)
         {
            p->output_->SetBin(
               radial, gate, 0, wsr88d::DataLevelCode::RangeFolded);
         }
         else
         {
            const float velocity =
               static_cast<float>(dataLevel) * dataScale + dataOffset;
            // Storm Relative Mean Radial Velocity = (Mean Radial Velocity) -
            // StrmSpd * cos(StrmDir - CurrAzm)
            // TODO should this angle be center, not edge?
            const float outputValue =
               velocity -
               meanStormSpeed * std::cos(meanStormDirection - startAngle);
            p->output_->SetBin(radial, gate, outputValue, std::nullopt);
         }
      }
   }

   return p->output_;
}

} // namespace scwx::deriver
