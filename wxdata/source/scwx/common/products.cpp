#include <scwx/common/products.hpp>

#include <unordered_map>

namespace scwx
{
namespace common
{

static const std::unordered_map<RadarProductGroup, std::string>
   radarProductGroupName_ {{RadarProductGroup::Level2, "L2"},
                           {RadarProductGroup::Level3, "L3"},
                           {RadarProductGroup::Unknown, "?"}};

static const std::unordered_map<Level2Product, std::string> level2Name_ {
   {Level2Product::Reflectivity, "REF"},
   {Level2Product::Velocity, "VEL"},
   {Level2Product::SpectrumWidth, "SW"},
   {Level2Product::DifferentialReflectivity, "ZDR"},
   {Level2Product::DifferentialPhase, "PHI"},
   {Level2Product::CorrelationCoefficient, "RHO"},
   {Level2Product::ClutterFilterPowerRemoved, "CFP"},
   {Level2Product::Unknown, "?"}};

static const std::unordered_map<Level2Product, std::string> level2Description_ {
   {Level2Product::Reflectivity, "Reflectivity"},
   {Level2Product::Velocity, "Velocity"},
   {Level2Product::SpectrumWidth, "Spectrum Width"},
   {Level2Product::DifferentialReflectivity, "Differential Reflectivity"},
   {Level2Product::DifferentialPhase, "Differential Phase"},
   {Level2Product::CorrelationCoefficient, "Correlation Coefficient"},
   {Level2Product::ClutterFilterPowerRemoved, "Clutter Filter Power Removed"},
   {Level2Product::Unknown, "?"}};

static const std::unordered_map<Level2Product, std::string> level2Palette_ {
   {Level2Product::Reflectivity, "BR"},
   {Level2Product::Velocity, "BV"},
   {Level2Product::SpectrumWidth, "SW"},
   {Level2Product::DifferentialReflectivity, "ZDR"},
   {Level2Product::DifferentialPhase, "PHI2"},
   {Level2Product::CorrelationCoefficient, "CC"},
   {Level2Product::ClutterFilterPowerRemoved, "???"},
   {Level2Product::Unknown, "???"}};

static const std::unordered_map<Level3ProductCategory, std::string>
   level3CategoryName_ {
      {Level3ProductCategory::Reflectivity, "REF"},
      {Level3ProductCategory::Velocity, "VEL"},
      {Level3ProductCategory::StormRelativeVelocity, "SRM"},
      {Level3ProductCategory::SpectrumWidth, "SW"},
      {Level3ProductCategory::DifferentialReflectivity, "ZDR"},
      {Level3ProductCategory::SpecificDifferentialPhase, "KDP"},
      {Level3ProductCategory::CorrelationCoefficient, "CC"},
      {Level3ProductCategory::Unknown, "?"}};

static const std::unordered_map<Level3ProductCategory, std::string>
   level3CategoryDescription_ {
      {Level3ProductCategory::Reflectivity, "Reflectivity"},
      {Level3ProductCategory::Velocity, "Velocity"},
      {Level3ProductCategory::StormRelativeVelocity, "Storm Relative Velocity"},
      {Level3ProductCategory::SpectrumWidth, "Spectrum Width"},
      {Level3ProductCategory::DifferentialReflectivity,
       "Differential Reflectivity"},
      {Level3ProductCategory::SpecificDifferentialPhase,
       "Specific Differential Phase"},
      {Level3ProductCategory::CorrelationCoefficient,
       "Correlation Coefficient"},
      {Level3ProductCategory::Unknown, "?"}};

static const std::unordered_map<Level3ProductCategory, std::string>
   level3CategoryDefaultProduct_ {
      {Level3ProductCategory::Reflectivity, "N0B"},
      {Level3ProductCategory::Velocity, "N0G"},
      {Level3ProductCategory::StormRelativeVelocity, "N0S"},
      {Level3ProductCategory::SpectrumWidth, "NSW"},
      {Level3ProductCategory::DifferentialReflectivity, "N0X"},
      {Level3ProductCategory::SpecificDifferentialPhase, "N0K"},
      {Level3ProductCategory::CorrelationCoefficient, "N0C"}};

static const std::unordered_map<int, std::string> level3Palette_ {
   {19, "BR"},     {20, "BR"},     {27, "BV"},     {30, "SW"},
   {31, "STPIN"},  {32, "BR"},     {37, "BR"},     {38, "BR"},
   {41, "ET"},     {50, "BR"},     {51, "BV"},     {56, "SRV"},
   {57, "VIL"},    {65, "BR"},     {66, "BR"},     {67, "BR"},
   {78, "OHPIN"},  {79, "OHPIN"},  {80, "STPIN"},  {81, "???"},
   {86, "BV"},     {90, "BR"},     {93, "BV"},     {94, "BR"},
   {97, "BR"},     {98, "BR"},     {99, "BV"},     {113, "???"},
   {132, "???"},   {133, "???"},   {134, "VIL"},   {135, "ET"},
   {137, "BR"},    {138, "STPIN"}, {144, "OHPIN"}, {145, "OHPIN"},
   {146, "STPIN"}, {150, "STPIN"}, {151, "STPIN"}, {153, "BR"},
   {154, "BV"},    {155, "SW"},    {159, "ZDR"},   {161, "CC"},
   {163, "PHI3"},  {165, "???"},   {167, "CC"},    {168, "PHI3"},
   {169, "OHPIN"}, {170, "STP"},   {171, "STPIN"}, {172, "STP"},
   {173, "STP"},   {174, "DOD"},   {175, "DSD"},   {176, "???"},
   {177, "???"},   {178, "???"},   {179, "???"},   {180, "BR"},
   {181, "BR"},    {182, "BV"},    {184, "SW"},    {186, "BR"},
   {193, "BR"},    {195, "BR"},    {-1, "???"}};

const std::string& GetRadarProductGroupName(RadarProductGroup group)
{
   return radarProductGroupName_.at(group);
}

RadarProductGroup GetRadarProductGroup(const std::string& name)
{
   auto result = std::find_if(
      radarProductGroupName_.cbegin(),
      radarProductGroupName_.cend(),
      [&](const std::pair<RadarProductGroup, std::string>& pair) -> bool
      { return pair.second == name; });

   if (result != radarProductGroupName_.cend())
   {
      return result->first;
   }
   else
   {
      return RadarProductGroup::Unknown;
   }
}

const std::string& GetLevel2Name(Level2Product product)
{
   return level2Name_.at(product);
}

const std::string& GetLevel2Description(Level2Product product)
{
   return level2Description_.at(product);
}

const std::string& GetLevel2Palette(Level2Product product)
{
   return level2Palette_.at(product);
}

Level2Product GetLevel2Product(const std::string& name)
{
   auto result = std::find_if(
      level2Name_.cbegin(),
      level2Name_.cend(),
      [&](const std::pair<Level2Product, std::string>& pair) -> bool
      { return pair.second == name; });

   if (result != level2Name_.cend())
   {
      return result->first;
   }
   else
   {
      return Level2Product::Unknown;
   }
}

const std::string& GetLevel3CategoryName(Level3ProductCategory category)
{
   return level3CategoryName_.at(category);
}

const std::string& GetLevel3CategoryDescription(Level3ProductCategory category)
{
   return level3CategoryDescription_.at(category);
}

const std::string&
GetLevel3CategoryDefaultProduct(Level3ProductCategory category)
{
   return level3CategoryDefaultProduct_.at(category);
}

Level3ProductCategory GetLevel3Category(const std::string& categoryName)
{
   auto result = std::find_if(
      level3CategoryName_.cbegin(),
      level3CategoryName_.cend(),
      [&](const std::pair<Level3ProductCategory, std::string>& pair) -> bool
      { return pair.second == categoryName; });

   if (result != level3CategoryName_.cend())
   {
      return result->first;
   }
   else
   {
      return Level3ProductCategory::Unknown;
   }
}

Level3ProductCategory GetLevel3CategoryByProduct(const std::string& productName)
{
   auto result = std::find_if(
      level3CategoryDefaultProduct_.cbegin(),
      level3CategoryDefaultProduct_.cend(),
      [&](const std::pair<Level3ProductCategory, std::string>& pair) -> bool
      { return pair.second == productName; });

   if (result != level3CategoryDefaultProduct_.cend())
   {
      return result->first;
   }
   else
   {
      return Level3ProductCategory::Unknown;
   }
}

const std::string& GetLevel3Palette(int16_t productCode)
{
   auto it = level3Palette_.find(productCode);
   if (it != level3Palette_.cend())
   {
      return it->second;
   }
   else
   {
      return level3Palette_.at(-1);
   }
}

} // namespace common
} // namespace scwx
