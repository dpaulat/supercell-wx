#include <scwx/wsr88d/wsr88d_types.hpp>

#include <unordered_map>

namespace scwx
{
namespace wsr88d
{

static const std::unordered_map<DataLevelCode, std::string> dataLevelCodeName_ {
   {DataLevelCode::BadData, "Bad Data"},
   {DataLevelCode::BelowThreshold, "Below Threshold"},
   {DataLevelCode::Blank, ""},
   {DataLevelCode::ChaffDetection, "Chaff Detection"},
   {DataLevelCode::EditRemove, "Edit/Remove"},
   {DataLevelCode::FlaggedData, "Flagged Data"},
   {DataLevelCode::Missing, "Missing"},
   {DataLevelCode::NoData, "No Data"},
   {DataLevelCode::OutsideCoverageArea, "Data Outside Coverage Area"},
   {DataLevelCode::NoAccumulation, "No Accumulation"},
   {DataLevelCode::RangeFolded, "Range Folded"},
   {DataLevelCode::Reserved, "Reserved"},

   // Hydrometeor Classification
   {DataLevelCode::Biological, "Biological"},
   {DataLevelCode::AnomalousPropagationGroundClutter,
    "Anomalous Propagation/Ground Clutter"},
   {DataLevelCode::IceCrystals, "Ice Crystals"},
   {DataLevelCode::DrySnow, "Dry Snow"},
   {DataLevelCode::WetSnow, "Wet Snow"},
   {DataLevelCode::LightAndOrModerateRain, "Light/Moderate Rain"},
   {DataLevelCode::HeavyRain, "Heavy Rain"},
   {DataLevelCode::BigDrops, "Big Drops"},
   {DataLevelCode::Graupel, "Graupel"},
   {DataLevelCode::SmallHail, "Small Hail"},
   {DataLevelCode::LargeHail, "Large Hail"},
   {DataLevelCode::GiantHail, "Giant Hail"},
   {DataLevelCode::UnknownClassification, "Unknown Classification"},

   // Rainfall Rate Classification
   {DataLevelCode::NoPrecipitation, "No Precip (Biota or NoEcho)"},
   {DataLevelCode::Unfilled, "Unfilled"},
   {DataLevelCode::Convective, "Convective R(Z,ZDR)"},
   {DataLevelCode::Tropical, "Tropical R(Z,ZDR)"},
   {DataLevelCode::SpecificAttenuation, "Specific Attenuation"},
   {DataLevelCode::KL, "R(KDP) 25 Coeff."},
   {DataLevelCode::KH, "R(KDP) 44 Coeff."},
   {DataLevelCode::Z1, "R(Z)"},
   {DataLevelCode::Z6, "R(Z) * 0.6"},
   {DataLevelCode::Z8, "R(Z) * 0.8"},
   {DataLevelCode::SI, "R(Z) * multiplier"},

   {DataLevelCode::Unknown, "?"}};

static const std::unordered_map<DataLevelCode, std::string>
   dataLevelCodeShortName_ {
      {DataLevelCode::BadData, ""},
      {DataLevelCode::BelowThreshold, "TH"},
      {DataLevelCode::Blank, ""},
      {DataLevelCode::ChaffDetection, ""},
      {DataLevelCode::EditRemove, ""},
      {DataLevelCode::FlaggedData, ""},
      {DataLevelCode::Missing, ""},
      {DataLevelCode::NoData, "ND"},
      {DataLevelCode::OutsideCoverageArea, ""},
      {DataLevelCode::NoAccumulation, ""},
      {DataLevelCode::RangeFolded, "RF"},
      {DataLevelCode::Reserved, ""},

      // Hydrometeor Classification
      {DataLevelCode::Biological, "BI"},
      {DataLevelCode::AnomalousPropagationGroundClutter, "GC"},
      {DataLevelCode::IceCrystals, "IC"},
      {DataLevelCode::DrySnow, "DS"},
      {DataLevelCode::WetSnow, "WS"},
      {DataLevelCode::LightAndOrModerateRain, "RA"},
      {DataLevelCode::HeavyRain, "HR"},
      {DataLevelCode::BigDrops, "BD"},
      {DataLevelCode::Graupel, "GR"},
      {DataLevelCode::SmallHail, "HA"},
      {DataLevelCode::LargeHail, "LH"},
      {DataLevelCode::GiantHail, "GH"},
      {DataLevelCode::UnknownClassification, "UK"},

      // Rainfall Rate Classification
      {DataLevelCode::NoPrecipitation, "NP"},
      {DataLevelCode::Unfilled, "UF"},
      {DataLevelCode::Convective, "CZ"},
      {DataLevelCode::Tropical, "TZ"},
      {DataLevelCode::SpecificAttenuation, "SA"},
      {DataLevelCode::KL, "KL"},
      {DataLevelCode::KH, "KH"},
      {DataLevelCode::Z1, "Z1"},
      {DataLevelCode::Z6, "Z6"},
      {DataLevelCode::Z8, "Z8"},
      {DataLevelCode::SI, "SI"},

      {DataLevelCode::Unknown, "?"}};

const std::string& GetDataLevelCodeName(DataLevelCode dataLevelCode)
{
   return dataLevelCodeName_.at(dataLevelCode);
}

const std::string& GetDataLevelCodeShortName(DataLevelCode dataLevelCode)
{
   return dataLevelCodeShortName_.at(dataLevelCode);
}

} // namespace wsr88d
} // namespace scwx
