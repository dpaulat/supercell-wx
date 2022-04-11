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

static const std::unordered_map<int16_t, std::string> level3Palette_ {
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

} // namespace common
} // namespace scwx
