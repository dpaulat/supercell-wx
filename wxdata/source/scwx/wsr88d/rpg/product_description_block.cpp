#include <scwx/wsr88d/rpg/product_description_block.hpp>

#include <array>
#include <istream>
#include <set>
#include <string>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ =
   "[scwx::wsr88d::rpg::product_description_block] ";

static const std::set<int16_t> compressedProducts_ = {
   32,  94,  99,  134, 135, 138, 149, 152, 153, 154, 155,
   159, 161, 163, 165, 167, 168, 170, 172, 173, 174, 175,
   176, 177, 178, 179, 180, 182, 186, 193, 195, 202};

static const std::unordered_map<int16_t, uint16_t> rangeMap_ {
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
   {176, 230}, {177, 230}, {178, 300}, {179, 300}, {193, 460}, {195, 460},
   {196, 50}};

static const std::unordered_map<int16_t, uint16_t> xResolutionMap_ {
   {19, 1000},  {20, 2000},  {27, 1000},  {30, 1000},  {31, 2000},  {32, 1000},
   {37, 1000},  {38, 4000},  {41, 4000},  {50, 1000},  {51, 1000},  {56, 1000},
   {57, 4000},  {65, 4000},  {66, 4000},  {67, 4000},  {78, 2000},  {79, 2000},
   {80, 2000},  {86, 1000},  {90, 4000},  {93, 1000},  {94, 1000},  {97, 1000},
   {98, 1000},  {99, 250},   {113, 250},  {132, 1000}, {133, 1000}, {134, 1000},
   {135, 1000}, {137, 1000}, {138, 2000}, {144, 1000}, {145, 1000}, {146, 1000},
   {147, 1000}, {150, 1000}, {151, 1000}, {153, 250},  {154, 250},  {155, 250},
   {159, 250},  {161, 250},  {163, 250},  {165, 250},  {166, 250},  {167, 250},
   {168, 250},  {169, 2000}, {170, 250},  {171, 2000}, {172, 250},  {173, 250},
   {174, 250},  {175, 250},  {176, 250},  {177, 250},  {178, 1000}, {179, 1000},
   {193, 250},  {195, 1000}};

static const std::unordered_map<int16_t, uint16_t> yResolutionMap_ {{37, 1000},
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
       halfword31_ {0},
       halfword32_ {0},
       halfword33_ {0},
       halfword34_ {0},
       halfword35_ {0},
       version_ {0},
       spotBlank_ {0},
       offsetToSymbology_ {0},
       offsetToGraphic_ {0},
       offsetToTabular_ {0},
       parameters_ {0}
   {
   }
   ~ProductDescriptionBlockImpl() = default;

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
   uint16_t halfword31_;
   uint16_t halfword32_;
   uint16_t halfword33_;
   uint16_t halfword34_;
   uint16_t halfword35_;
   // Halfwords 36-46 are unused
   // 47-53: Product dependent parameters 4-10 (Table V, Note 3)
   uint8_t  version_;
   uint8_t  spotBlank_;
   uint32_t offsetToSymbology_;
   uint32_t offsetToGraphic_;
   uint32_t offsetToTabular_;

   std::array<uint16_t, 10> parameters_;
};

ProductDescriptionBlock::ProductDescriptionBlock() :
    p(std::make_unique<ProductDescriptionBlockImpl>())
{
}
ProductDescriptionBlock::~ProductDescriptionBlock() = default;

ProductDescriptionBlock::ProductDescriptionBlock(
   ProductDescriptionBlock&&) noexcept                    = default;
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
      range = it->second;
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
      xResolution = it->second;
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
      yResolution = it->second;
   }

   return yResolution;
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
   is.read(reinterpret_cast<char*>(&p->halfword31_), 2);              // 31
   is.read(reinterpret_cast<char*>(&p->halfword32_), 2);              // 32
   is.read(reinterpret_cast<char*>(&p->halfword33_), 2);              // 33
   is.read(reinterpret_cast<char*>(&p->halfword34_), 2);              // 34
   is.read(reinterpret_cast<char*>(&p->halfword35_), 2);              // 35

   is.seekg(11 * 2, std::ios_base::cur); // 36-46

   is.read(reinterpret_cast<char*>(&p->parameters_[3]), 7 * 2); // 47-53
   is.read(reinterpret_cast<char*>(&p->version_), 1);           // 54
   is.read(reinterpret_cast<char*>(&p->spotBlank_), 1);         // 54
   is.read(reinterpret_cast<char*>(&p->offsetToSymbology_), 4); // 55-56
   is.read(reinterpret_cast<char*>(&p->offsetToGraphic_), 4);   // 57-58
   is.read(reinterpret_cast<char*>(&p->offsetToTabular_), 4);   // 59-60

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
   p->halfword31_              = ntohs(p->halfword31_);
   p->halfword32_              = ntohs(p->halfword32_);
   p->halfword33_              = ntohs(p->halfword33_);
   p->halfword34_              = ntohs(p->halfword34_);
   p->halfword35_              = ntohs(p->halfword35_);
   p->offsetToSymbology_       = ntohl(p->offsetToSymbology_);
   p->offsetToGraphic_         = ntohl(p->offsetToGraphic_);
   p->offsetToTabular_         = ntohl(p->offsetToTabular_);

   SwapArray(p->parameters_);

   if (is.eof())
   {
      BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Reached end of file";
      blockValid = false;
   }
   else
   {
      if (p->blockDivider_ != -1)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid block divider: " << p->blockDivider_;
         blockValid = false;
      }
      if (p->productCode_ < -299 ||
          (p->productCode_ > -16 && p->productCode_ < 16) ||
          p->productCode_ > 299)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid product code: " << p->productCode_;
         blockValid = false;
      }
   }

   if (blockValid)
   {
      BOOST_LOG_TRIVIAL(trace)
         << logPrefix_ << "Product code: " << p->productCode_;
   }

   const std::streampos blockEnd = is.tellg();
   if (!ValidateMessage(is, blockEnd - blockStart))
   {
      blockValid = false;
   }

   return blockValid;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
