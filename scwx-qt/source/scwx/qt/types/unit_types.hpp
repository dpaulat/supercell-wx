#pragma once

#include <scwx/util/iterator.hpp>

#include <cstdint>
#include <string>

namespace scwx::qt::types
{

enum class AccumulationUnits : std::uint8_t
{
   Inches,
   Millimeters,
   User,
   Unknown
};
using AccumulationUnitsIterator =
   scwx::util::Iterator<AccumulationUnits,
                        AccumulationUnits::Inches,
                        AccumulationUnits::User>;

enum class EchoTopsUnits : std::uint8_t
{
   Kilofeet,
   Kilometers,
   User,
   Unknown
};
using EchoTopsUnitsIterator = scwx::util::
   Iterator<EchoTopsUnits, EchoTopsUnits::Kilofeet, EchoTopsUnits::User>;

enum class OtherUnits : std::uint8_t
{
   Default,
   User,
   Unknown
};
using OtherUnitsIterator =
   scwx::util::Iterator<OtherUnits, OtherUnits::Default, OtherUnits::User>;

enum class SpeedUnits : std::uint8_t
{
   KilometersPerHour,
   Knots,
   MilesPerHour,
   MetersPerSecond,
   User,
   Unknown
};
using SpeedUnitsIterator = scwx::util::
   Iterator<SpeedUnits, SpeedUnits::KilometersPerHour, SpeedUnits::User>;

enum class DistanceUnits : std::uint8_t
{
   Kilometers,
   Miles,
   User,
   Unknown
};
using DistanceUnitsIterator = scwx::util::
   Iterator<DistanceUnits, DistanceUnits::Kilometers, DistanceUnits::User>;

enum class RadarBeamHeightReference : std::uint8_t
{
   AboveRadarLevel,
   MeanSeaLevel,
   Unknown
};
using RadarBeamHeightReferenceIterator =
   scwx::util::Iterator<RadarBeamHeightReference,
                        RadarBeamHeightReference::AboveRadarLevel,
                        RadarBeamHeightReference::MeanSeaLevel>;

const std::string& GetAccumulationUnitsAbbreviation(AccumulationUnits units);
const std::string& GetAccumulationUnitsName(AccumulationUnits units);
AccumulationUnits  GetAccumulationUnitsFromName(const std::string& name);
float              GetAccumulationUnitsScale(AccumulationUnits units);

const std::string& GetEchoTopsUnitsAbbreviation(EchoTopsUnits units);
const std::string& GetEchoTopsUnitsName(EchoTopsUnits units);
EchoTopsUnits      GetEchoTopsUnitsFromName(const std::string& name);
float              GetEchoTopsUnitsScale(EchoTopsUnits units);

const std::string& GetOtherUnitsName(OtherUnits units);
OtherUnits         GetOtherUnitsFromName(const std::string& name);

const std::string& GetSpeedUnitsAbbreviation(SpeedUnits units);
const std::string& GetSpeedUnitsName(SpeedUnits units);
SpeedUnits         GetSpeedUnitsFromName(const std::string& name);
float              GetSpeedUnitsScale(SpeedUnits units);

const std::string& GetDistanceUnitsAbbreviation(DistanceUnits units);
const std::string& GetDistanceUnitsName(DistanceUnits units);
DistanceUnits      GetDistanceUnitsFromName(const std::string& name);
double             GetDistanceUnitsScale(DistanceUnits units);

const std::string&
GetRadarBeamHeightReferenceAbbreviation(RadarBeamHeightReference reference);
const std::string&
GetRadarBeamHeightReferenceName(RadarBeamHeightReference reference);
RadarBeamHeightReference
GetRadarBeamHeightReferenceFromName(const std::string& name);

} // namespace scwx::qt::types
