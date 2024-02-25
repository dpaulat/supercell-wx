#include <scwx/wsr88d/rpg/product_description_block.hpp>
#include <scwx/util/float.hpp>
#include <scwx/util/logger.hpp>

#include <array>
#include <istream>
#include <numbers>
#include <set>
#include <string>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ =
   "scwx::wsr88d::rpg::product_description_block";
static const auto logger_ = util::Logger::Create(logPrefix_);

static const std::set<int> compressedProducts_ = {
   32,  94,  99,  134, 135, 138, 149, 152, 153, 154, 155,
   159, 161, 163, 165, 167, 168, 170, 172, 173, 174, 175,
   176, 177, 178, 179, 180, 182, 186, 193, 195, 202};

static const std::set<int> uncodedDataLevelProducts_ = {32,
                                                        34,
                                                        81,
                                                        93,
                                                        94,
                                                        99,
                                                        134,
                                                        135,
                                                        138,
                                                        153,
                                                        154,
                                                        155,
                                                        159,
                                                        161,
                                                        163,
                                                        177,
                                                        193,
                                                        195};

static const std::unordered_map<int, unsigned int> rangeMap_ {
   {19, 230},  {20, 460},  {27, 230},  {30, 230},  {31, 230},  {32, 230},
   {37, 230},  {38, 460},  {41, 230},  {50, 230},  {51, 230},  {56, 230},
   {57, 230},  {58, 460},  {59, 230},  {61, 230},  {62, 460},  {65, 230},
   {66, 230},  {67, 230},  {74, 460},  {78, 230},  {79, 230},  {80, 230},
   {81, 230},  {86, 230},  {90, 230},  {93, 115},  {94, 460},  {97, 230},
   {98, 460},  {99, 230},  {113, 300}, {132, 230}, {133, 230}, {134, 460},
   {135, 345}, {137, 230}, {138, 230}, {140, 70},  {141, 230}, {143, 230},
   {144, 230}, {145, 230}, {146, 230}, {147, 230}, {149, 230}, {150, 230},
   {151, 230}, {153, 460}, {154, 300}, {155, 300}, {159, 300}, {161, 300},
   {163, 300}, {165, 300}, {166, 230}, {167, 300}, {168, 300}, {169, 230},
   {170, 230}, {171, 230}, {172, 230}, {173, 230}, {174, 230}, {175, 230},
   {176, 230}, {177, 230}, {178, 300}, {179, 300}, {180, 89},  {181, 89},
   {182, 89},  {184, 89},  {186, 412}, {193, 460}, {195, 460}, {196, 50}};

static const std::unordered_map<int, unsigned int> xResolutionMap_ {
   {19, 1000},  {20, 2000},  {27, 1000},  {30, 1000},  {31, 2000},  {32, 1000},
   {33, 1000},  {34, 1000},  {37, 1000},  {38, 4000},  {41, 4000},  {50, 1000},
   {51, 1000},  {56, 1000},  {57, 4000},  {65, 4000},  {66, 4000},  {67, 4000},
   {78, 2000},  {79, 2000},  {80, 2000},  {86, 1000},  {90, 4000},  {93, 1000},
   {94, 1000},  {97, 1000},  {98, 1000},  {99, 250},   {113, 250},  {132, 1000},
   {133, 1000}, {134, 1000}, {135, 1000}, {137, 1000}, {138, 2000}, {144, 1000},
   {145, 1000}, {146, 1000}, {147, 1000}, {150, 1000}, {151, 1000}, {153, 250},
   {154, 250},  {155, 250},  {159, 250},  {161, 250},  {163, 250},  {165, 250},
   {166, 250},  {167, 250},  {168, 250},  {169, 2000}, {170, 250},  {171, 2000},
   {172, 250},  {173, 250},  {174, 250},  {175, 250},  {176, 250},  {177, 250},
   {178, 1000}, {179, 1000}, {180, 150},  {181, 150},  {182, 150},  {184, 150},
   {186, 300},  {193, 250},  {195, 1000}};

static const std::unordered_map<int, unsigned int> yResolutionMap_ {{37, 1000},
                                                                    {38, 4000},
                                                                    {41, 4000},
                                                                    {50, 500},
                                                                    {51, 500},
                                                                    {57, 4000},
                                                                    {65, 4000},
                                                                    {66, 4000},
                                                                    {67, 4000},
                                                                    {86, 500},
                                                                    {90, 4000},
                                                                    {97, 1000},
                                                                    {98, 4000},
                                                                    {166, 250}};

class ProductDescriptionBlockImpl
{
public:
   explicit ProductDescriptionBlockImpl() :
       blockDivider_ {0},
       latitudeOfRadar_ {0},
       longitudeOfRadar_ {0},
       heightOfRadar_ {0},
       productCode_ {0},
       operationalMode_ {0},
       volumeCoveragePattern_ {0},
       sequenceNumber_ {0},
       volumeScanNumber_ {0},
       volumeScanDate_ {0},
       volumeScanStartTime_ {0},
       generationDateOfProduct_ {0},
       generationTimeOfProduct_ {0},
       elevationNumber_ {0},
       version_ {0},
       spotBlank_ {0},
       offsetToSymbology_ {0},
       offsetToGraphic_ {0},
       offsetToTabular_ {0},
       parameters_ {0},
       halfwords_ {0}
   {
   }
   ~ProductDescriptionBlockImpl() = default;

   uint16_t halfword(size_t i);

   int16_t  blockDivider_;
   int32_t  latitudeOfRadar_;
   int32_t  longitudeOfRadar_;
   int16_t  heightOfRadar_;
   int16_t  productCode_;
   uint16_t operationalMode_;
   uint16_t volumeCoveragePattern_;
   int16_t  sequenceNumber_;
   uint16_t volumeScanNumber_;
   uint16_t volumeScanDate_;
   uint32_t volumeScanStartTime_;
   uint16_t generationDateOfProduct_;
   uint32_t generationTimeOfProduct_;
   // 27-28: Product dependent parameters 1 and 2 (Table V)
   uint16_t elevationNumber_;
   // 30:    Product dependent parameter 3 (Table V)
   // 31-46: Product dependent (Note 1)
   // 47-53: Product dependent parameters 4-10 (Table V, Note 3)
   uint8_t  version_;
   uint8_t  spotBlank_;
   uint32_t offsetToSymbology_;
   uint32_t offsetToGraphic_;
   uint32_t offsetToTabular_;

   std::array<uint16_t, 10> parameters_;
   std::array<uint16_t, 16> halfwords_;
};

uint16_t ProductDescriptionBlockImpl::halfword(size_t i)
{
   // Halfwords start at halfword 31
   return halfwords_[i - 31];
}

ProductDescriptionBlock::ProductDescriptionBlock() :
    p(std::make_unique<ProductDescriptionBlockImpl>())
{
}
ProductDescriptionBlock::~ProductDescriptionBlock() = default;

ProductDescriptionBlock::ProductDescriptionBlock(
   ProductDescriptionBlock&&) noexcept = default;
ProductDescriptionBlock& ProductDescriptionBlock::operator=(
   ProductDescriptionBlock&&) noexcept = default;

int16_t ProductDescriptionBlock::block_divider() const
{
   return p->blockDivider_;
}

float ProductDescriptionBlock::latitude_of_radar() const
{
   return p->latitudeOfRadar_ * 0.001f;
}

float ProductDescriptionBlock::longitude_of_radar() const
{
   return p->longitudeOfRadar_ * 0.001f;
}

int16_t ProductDescriptionBlock::height_of_radar() const
{
   return p->heightOfRadar_;
}

int16_t ProductDescriptionBlock::product_code() const
{
   return p->productCode_;
}

uint16_t ProductDescriptionBlock::operational_mode() const
{
   return p->operationalMode_;
}

uint16_t ProductDescriptionBlock::volume_coverage_pattern() const
{
   return p->volumeCoveragePattern_;
}

int16_t ProductDescriptionBlock::sequence_number() const
{
   return p->sequenceNumber_;
}

uint16_t ProductDescriptionBlock::volume_scan_number() const
{
   return p->volumeScanNumber_;
}

uint16_t ProductDescriptionBlock::volume_scan_date() const
{
   return p->volumeScanDate_;
}

uint32_t ProductDescriptionBlock::volume_scan_start_time() const
{
   return p->volumeScanStartTime_;
}

uint16_t ProductDescriptionBlock::generation_date_of_product() const
{
   return p->generationDateOfProduct_;
}

uint32_t ProductDescriptionBlock::generation_time_of_product() const
{
   return p->generationTimeOfProduct_;
}

uint16_t ProductDescriptionBlock::elevation_number() const
{
   return p->elevationNumber_;
}

uint16_t ProductDescriptionBlock::data_level_threshold(size_t i) const
{
   return p->halfwords_[i];
}

uint8_t ProductDescriptionBlock::version() const
{
   return p->version_;
}

uint8_t ProductDescriptionBlock::spot_blank() const
{
   return p->spotBlank_;
}

uint32_t ProductDescriptionBlock::offset_to_symbology() const
{
   return p->offsetToSymbology_;
}

uint32_t ProductDescriptionBlock::offset_to_graphic() const
{
   return p->offsetToGraphic_;
}

uint32_t ProductDescriptionBlock::offset_to_tabular() const
{
   return p->offsetToTabular_;
}

float ProductDescriptionBlock::range() const
{
   return range_raw();
}

uint16_t ProductDescriptionBlock::range_raw() const
{
   uint16_t range = 0;

   auto it = rangeMap_.find(p->productCode_);
   if (it != rangeMap_.cend())
   {
      range = static_cast<uint16_t>(it->second);
   }

   return range;
}

float ProductDescriptionBlock::x_resolution() const
{
   return x_resolution_raw() * 0.001f;
}

uint16_t ProductDescriptionBlock::x_resolution_raw() const
{
   uint16_t xResolution = 0;

   auto it = xResolutionMap_.find(p->productCode_);
   if (it != xResolutionMap_.cend())
   {
      xResolution = static_cast<uint16_t>(it->second);
   }

   return xResolution;
}

float ProductDescriptionBlock::y_resolution() const
{
   return y_resolution_raw() * 0.001f;
}

uint16_t ProductDescriptionBlock::y_resolution_raw() const
{
   uint16_t yResolution = 0;

   auto it = yResolutionMap_.find(p->productCode_);
   if (it != yResolutionMap_.cend())
   {
      yResolution = static_cast<uint16_t>(it->second);
   }

   return yResolution;
}

uint16_t ProductDescriptionBlock::threshold() const
{
   uint16_t threshold = 1;

   switch (p->productCode_)
   {
   case 81:
      threshold = 1;
      break;

   case 32:
   case 93:
   case 94:
   case 99:
   case 135:
   case 153:
   case 154:
   case 180:
   case 182:
   case 186:
   case 195:
      threshold = 2;
      break;

   case 138:
      threshold = p->halfword(31);
      break;

   case 155:
      threshold = 129;
      break;

   case 193:
      threshold = 3;
      break;

   case 159:
   case 161:
   case 163:
   case 167:
   case 168:
   case 170:
   case 172:
   case 173:
   case 174:
   case 175:
   case 176:
      threshold = p->halfword(37);
      break;

   case 165:
   case 177:
      threshold = 10;
      break;
   }

   return threshold;
}

float ProductDescriptionBlock::offset() const
{
   float offset = 0.0f;

   switch (p->productCode_)
   {
   case 32:
   case 81:
   case 93:
   case 94:
   case 99:
   case 153:
   case 154:
   case 155:
   case 180:
   case 182:
   case 186:
   case 193:
   case 195:
      offset = static_cast<int16_t>(p->halfword(31)) * 0.1f;
      break;

   case 134:
      offset = util::DecodeFloat16(p->halfword(32));
      break;

   case 135:
      offset = static_cast<int16_t>(p->halfword(33));
      break;

   case 159:
   case 161:
   case 163:
   case 167:
   case 168:
   case 170:
   case 172:
   case 173:
   case 174:
   case 175:
   case 176:
      offset = util::DecodeFloat32(p->halfword(33), p->halfword(34));
      break;
   }

   return offset;
}

float ProductDescriptionBlock::scale() const
{
   float scale = 1.0f;

   switch (p->productCode_)
   {
   case 32:
   case 93:
   case 94:
   case 99:
   case 153:
   case 154:
   case 155:
   case 180:
   case 182:
   case 186:
   case 193:
   case 195:
      scale = p->halfword(32) * 0.1f;
      break;

   case 81:
      scale = p->halfword(32) * 0.001f;
      break;

   case 134:
      scale = util::DecodeFloat16(p->halfword(31));
      break;

   case 135:
      scale = p->halfword(32);
      break;

   case 138:
      scale = p->halfword(32) * 0.01f;
      break;

   case 159:
   case 161:
   case 163:
   case 167:
   case 168:
   case 170:
   case 172:
   case 173:
   case 174:
   case 175:
   case 176:
      scale = util::DecodeFloat32(p->halfword(31), p->halfword(32));
      break;
   }

   return scale;
}

uint16_t ProductDescriptionBlock::number_of_levels() const
{
   uint16_t numberOfLevels = 16u;

   switch (p->productCode_)
   {
   case 19:
   case 20:
   case 27:
   case 31:
   case 37:
   case 38:
   case 41:
   case 49:
   case 50:
   case 51:
   case 56:
   case 57:
   case 78:
   case 79:
   case 80:
   case 97:
   case 98:
   case 137:
   case 144:
   case 145:
   case 146:
   case 147:
   case 150:
   case 151:
   case 169:
   case 171:
      numberOfLevels = 16;
      break;

   case 30:
   case 65:
   case 66:
   case 67:
   case 84:
   case 86:
   case 90:
      numberOfLevels = 8;
      break;

   case 32:
   case 81:
   case 93:
   case 94:
   case 99:
   case 153:
   case 154:
   case 155:
   case 180:
   case 182:
   case 186:
   case 193:
   case 195:
      numberOfLevels = p->halfword(33);
      break;

   case 113:
      numberOfLevels = 13;
      break;

   case 132:
      numberOfLevels = 11;
      break;

   case 133:
      numberOfLevels = 12;
      break;

   case 134:
      numberOfLevels = 256;
      break;

   case 135:
      numberOfLevels = 200;
      break;

   case 138:
      numberOfLevels = p->halfword(33);
      break;

   case 159:
   case 161:
   case 163:
   case 167:
   case 168:
   case 170:
   case 172:
   case 173:
   case 174:
   case 175:
   case 176:
      numberOfLevels = p->halfword(36);
      break;

   case 165:
   case 177:
      numberOfLevels = 160;
      break;

   case 178:
   case 179:
      numberOfLevels = 71;
      break;
   }

   return numberOfLevels;
}

std::uint16_t ProductDescriptionBlock::log_start() const
{
   std::uint16_t logStart = std::numeric_limits<std::uint16_t>::max();

   switch (p->productCode_)
   {
   case 134:
      logStart = p->halfword(33);
      break;
   }

   return logStart;
}

float ProductDescriptionBlock::log_offset() const
{
   float logOffset = 0.0f;

   switch (p->productCode_)
   {
   case 134:
      logOffset = util::DecodeFloat16(p->halfword(35));
      break;
   }

   return logOffset;
}

float ProductDescriptionBlock::log_scale() const
{
   float logScale = 1.0f;

   switch (p->productCode_)
   {
   case 134:
      logScale = util::DecodeFloat16(p->halfword(34));
      break;
   }

   return logScale;
}

units::angle::degrees<double> ProductDescriptionBlock::elevation() const
{
   double elevation = 0.0;

   if (p->elevationNumber_ > 0)
   {
      elevation = p->parameters_[2] * 0.1;
   }

   return units::angle::degrees<double> {elevation};
}

bool ProductDescriptionBlock::IsCompressionEnabled() const
{
   bool isCompressed = false;

   if (compressedProducts_.contains(p->productCode_))
   {
      isCompressed = (p->parameters_[7] == 1u);
   }

   return isCompressed;
}

bool ProductDescriptionBlock::IsDataLevelCoded() const
{
   return !uncodedDataLevelProducts_.contains(p->productCode_);
}

size_t ProductDescriptionBlock::data_size() const
{
   return SIZE;
}

bool ProductDescriptionBlock::Parse(std::istream& is)
{
   bool blockValid = true;

   const std::streampos blockStart = is.tellg();

   is.read(reinterpret_cast<char*>(&p->blockDivider_), 2);            // 10
   is.read(reinterpret_cast<char*>(&p->latitudeOfRadar_), 4);         // 11-12
   is.read(reinterpret_cast<char*>(&p->longitudeOfRadar_), 4);        // 13-14
   is.read(reinterpret_cast<char*>(&p->heightOfRadar_), 2);           // 15
   is.read(reinterpret_cast<char*>(&p->productCode_), 2);             // 16
   is.read(reinterpret_cast<char*>(&p->operationalMode_), 2);         // 17
   is.read(reinterpret_cast<char*>(&p->volumeCoveragePattern_), 2);   // 18
   is.read(reinterpret_cast<char*>(&p->sequenceNumber_), 2);          // 19
   is.read(reinterpret_cast<char*>(&p->volumeScanNumber_), 2);        // 20
   is.read(reinterpret_cast<char*>(&p->volumeScanDate_), 2);          // 21
   is.read(reinterpret_cast<char*>(&p->volumeScanStartTime_), 4);     // 22-23
   is.read(reinterpret_cast<char*>(&p->generationDateOfProduct_), 2); // 24
   is.read(reinterpret_cast<char*>(&p->generationTimeOfProduct_), 4); // 25-26
   is.read(reinterpret_cast<char*>(&p->parameters_[0]), 2 * 2);       // 27-28
   is.read(reinterpret_cast<char*>(&p->elevationNumber_), 2);         // 29
   is.read(reinterpret_cast<char*>(&p->parameters_[2]), 2);           // 30
   is.read(reinterpret_cast<char*>(&p->halfwords_[0]), 16 * 2);       // 31-46
   is.read(reinterpret_cast<char*>(&p->parameters_[3]), 7 * 2);       // 47-53
   is.read(reinterpret_cast<char*>(&p->version_), 1);                 // 54
   is.read(reinterpret_cast<char*>(&p->spotBlank_), 1);               // 54
   is.read(reinterpret_cast<char*>(&p->offsetToSymbology_), 4);       // 55-56
   is.read(reinterpret_cast<char*>(&p->offsetToGraphic_), 4);         // 57-58
   is.read(reinterpret_cast<char*>(&p->offsetToTabular_), 4);         // 59-60

   p->blockDivider_            = ntohs(p->blockDivider_);
   p->latitudeOfRadar_         = ntohl(p->latitudeOfRadar_);
   p->longitudeOfRadar_        = ntohl(p->longitudeOfRadar_);
   p->heightOfRadar_           = ntohs(p->heightOfRadar_);
   p->productCode_             = ntohs(p->productCode_);
   p->operationalMode_         = ntohs(p->operationalMode_);
   p->volumeCoveragePattern_   = ntohs(p->volumeCoveragePattern_);
   p->sequenceNumber_          = ntohs(p->sequenceNumber_);
   p->volumeScanNumber_        = ntohs(p->volumeScanNumber_);
   p->volumeScanDate_          = ntohs(p->volumeScanDate_);
   p->volumeScanStartTime_     = ntohl(p->volumeScanStartTime_);
   p->generationDateOfProduct_ = ntohs(p->generationDateOfProduct_);
   p->generationTimeOfProduct_ = ntohl(p->generationTimeOfProduct_);
   p->elevationNumber_         = ntohs(p->elevationNumber_);
   p->offsetToSymbology_       = ntohl(p->offsetToSymbology_);
   p->offsetToGraphic_         = ntohl(p->offsetToGraphic_);
   p->offsetToTabular_         = ntohl(p->offsetToTabular_);

   SwapArray(p->parameters_);
   SwapArray(p->halfwords_);

   if (is.eof())
   {
      logger_->debug("Reached end of file");
      blockValid = false;
   }
   else
   {
      if (p->blockDivider_ != -1)
      {
         logger_->warn("Invalid block divider: {}", p->blockDivider_);
         blockValid = false;
      }
      if (p->productCode_ < -299 ||
          (p->productCode_ > -16 && p->productCode_ < 16) ||
          p->productCode_ > 299)
      {
         logger_->warn("Invalid product code: {}", p->productCode_);
         blockValid = false;
      }
   }

   if (blockValid)
   {
      logger_->trace("Product code: {}", p->productCode_);
   }

   const std::streampos blockEnd = is.tellg();
   if (!ValidateMessage(is, blockEnd - blockStart))
   {
      blockValid = false;
   }

   return blockValid;
}

std::optional<DataLevelCode>
ProductDescriptionBlock::data_level_code(std::uint8_t level) const
{
   switch (p->productCode_)
   {
   case 32:
   case 93:
   case 94:
   case 99:
   case 153:
   case 154:
   case 155:
   case 159:
   case 161:
   case 163:
   case 167:
   case 168:
   case 195:
      switch (level)
      {
      case 0:
         return DataLevelCode::BelowThreshold;
      case 1:
         return DataLevelCode::RangeFolded;
      default:
         break;
      }
      break;

   case 81:
      switch (level)
      {
      case 0:
         return DataLevelCode::NoAccumulation;
      case 255:
         return DataLevelCode::OutsideCoverageArea;
      default:
         break;
      }
      break;

   case 134:
      switch (level)
      {
      case 0:
         return DataLevelCode::BelowThreshold;
      case 1:
         return DataLevelCode::FlaggedData;
      case 255:
         return DataLevelCode::Reserved;
      default:
         break;
      }
      break;

   case 135:
      switch (level)
      {
      case 0:
         return DataLevelCode::BelowThreshold;
      case 1:
         return DataLevelCode::BadData;
      default:
         break;
      }
      break;

   case 138:
      switch (level)
      {
      case 0:
         return DataLevelCode::NoAccumulation;
      default:
         break;
      }
      break;

   case 165:
   case 177:
      switch (level)
      {
      case 0:
         return DataLevelCode::BelowThreshold;
      case 10:
         return DataLevelCode::Biological;
      case 20:
         return DataLevelCode::AnomalousPropagationGroundClutter;
      case 30:
         return DataLevelCode::IceCrystals;
      case 40:
         return DataLevelCode::DrySnow;
      case 50:
         return DataLevelCode::WetSnow;
      case 60:
         return DataLevelCode::LightAndOrModerateRain;
      case 70:
         return DataLevelCode::HeavyRain;
      case 80:
         return DataLevelCode::BigDrops;
      case 90:
         return DataLevelCode::Graupel;
      case 100:
         return DataLevelCode::SmallHail;
      case 110:
         return DataLevelCode::LargeHail;
      case 120:
         return DataLevelCode::GiantHail;
      case 140:
         return DataLevelCode::UnknownClassification;
      case 150:
         return DataLevelCode::RangeFolded;
      }
      break;

   case 170:
   case 172:
   case 173:
   case 174:
   case 175:
      switch (level)
      {
      case 0:
         return DataLevelCode::NoData;
      default:
         break;
      }
      break;

   case 193:
      switch (level)
      {
      case 0:
         return DataLevelCode::BelowThreshold;
      case 1:
         return DataLevelCode::RangeFolded;
      case 2:
         return DataLevelCode::EditRemove;
      case 254:
         return DataLevelCode::ChaffDetection;
      default:
         break;
      }
      break;

   case 197:
      switch (level)
      {
      case 0:
         return DataLevelCode::NoPrecipitation;
      case 10:
         return DataLevelCode::Unfilled;
      case 20:
         return DataLevelCode::Convective;
      case 30:
         return DataLevelCode::Tropical;
      case 40:
         return DataLevelCode::SpecificAttenuation;
      case 50:
         return DataLevelCode::KL;
      case 60:
         return DataLevelCode::KH;
      case 70:
         return DataLevelCode::Z1;
      case 80:
         return DataLevelCode::Z6;
      case 90:
         return DataLevelCode::Z8;
      case 100:
         return DataLevelCode::SI;
      default:
         break;
      }
      break;

   default:
      break;
   }

   // Different products use different scale/offset formulas
   if (number_of_levels() <= 16 && level < 16 &&
       !uncodedDataLevelProducts_.contains(p->productCode_))
   {
      uint16_t th = data_level_threshold(level);
      if ((th & 0x8000u))
      {
         // If bit 0 is one, then the LSB is coded
         uint16_t lsb = th & 0x00ffu;

         switch (lsb)
         {
         case 0:
            return DataLevelCode::Blank;
         case 1:
            return DataLevelCode::BelowThreshold;
         case 2:
            return DataLevelCode::NoData;
         case 3:
            return DataLevelCode::RangeFolded;
         case 4:
            return DataLevelCode::Biological;
         case 5:
            return DataLevelCode::AnomalousPropagationGroundClutter;
         case 6:
            return DataLevelCode::IceCrystals;
         case 7:
            return DataLevelCode::Graupel;
         case 8:
            return DataLevelCode::WetSnow;
         case 9:
            return DataLevelCode::DrySnow;
         case 10:
            return DataLevelCode::LightAndOrModerateRain;
         case 11:
            return DataLevelCode::HeavyRain;
         case 12:
            return DataLevelCode::BigDrops;
         case 13:
            return DataLevelCode::SmallHail;
         case 14:
            return DataLevelCode::UnknownClassification;
         case 15:
            return DataLevelCode::LargeHail;
         case 16:
            return DataLevelCode::GiantHail;
         default:
            break;
         }
      }
   }

   return std::nullopt;
}

std::optional<float>
ProductDescriptionBlock::data_value(std::uint8_t level) const
{
   float         dataOffset     = offset();
   float         dataScale      = scale();
   std::uint16_t dataThreshold  = threshold();
   std::uint16_t numberOfLevels = number_of_levels();

   if (level < dataThreshold)
   {
      return std::nullopt;
   }

   std::optional<float> f = std::nullopt;

   // Different products use different scale/offset formulas
   if (numberOfLevels > 16 ||
       uncodedDataLevelProducts_.contains(p->productCode_))
   {
      switch (p->productCode_)
      {
      case 159:
      case 161:
      case 163:
      case 167:
      case 168:
      case 170:
      case 172:
      case 173:
      case 174:
      case 175:
      case 176:
         f = (level - dataOffset) / dataScale;
         break;

      case 134:
         if (level < log_start())
         {
            f = (level - dataOffset) / dataScale;
         }
         else
         {
            f = static_cast<float>(std::pow<double>(
               std::numbers::e, (level - log_offset()) / log_scale()));
         }
         break;

      default:
         f = level * dataScale + dataOffset;
         break;
      }
   }
   else if (level < 16)
   {
      std::uint16_t th = data_level_threshold(level);
      if ((th & 0x8000u) == 0)
      {
         float scaleFactor = 1.0f;

         if (th & 0x4000u)
         {
            scaleFactor *= 0.01f;
         }
         if (th & 0x2000u)
         {
            scaleFactor *= 0.05f;
         }
         if (th & 0x1000u)
         {
            scaleFactor *= 0.1f;
         }
         if (th & 0x0100u)
         {
            scaleFactor *= -1.0f;
         }

         // If bit 0 is zero, then the LSB is numeric
         f = static_cast<float>(th & 0x00ffu) * scaleFactor;
      }
      else
      {
         // If bit 0 is one, then the LSB is coded
         std::uint16_t lsb = th & 0x00ffu;

         f = static_cast<float>(lsb);
      }
   }

   return f;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
