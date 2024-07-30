#pragma once

#include <scwx/util/iterator.hpp>

#include <string>

namespace scwx
{
namespace qt
{
namespace types
{

enum class AccumulationUnits
{
   Inches,
   Millimeters,
   User,
   Unknown
};
typedef scwx::util::Iterator<AccumulationUnits,
                             AccumulationUnits::Inches,
                             AccumulationUnits::User>
   AccumulationUnitsIterator;

enum class EchoTopsUnits
{
   Kilofeet,
   Kilometers,
   User,
   Unknown
};
typedef scwx::util::
   Iterator<EchoTopsUnits, EchoTopsUnits::Kilofeet, EchoTopsUnits::User>
      EchoTopsUnitsIterator;

enum class OtherUnits
{
   Default,
   User,
   Unknown
};
typedef scwx::util::Iterator<OtherUnits, OtherUnits::Default, OtherUnits::User>
   OtherUnitsIterator;

enum class SpeedUnits
{
   KilometersPerHour,
   Knots,
   MilesPerHour,
   MetersPerSecond,
   User,
   Unknown
};
typedef scwx::util::
   Iterator<SpeedUnits, SpeedUnits::KilometersPerHour, SpeedUnits::User>
      SpeedUnitsIterator;

enum class DistanceUnits
{
   Kilometers,
   Miles,
   User,
   Unknown
};
typedef scwx::util::
   Iterator<DistanceUnits, DistanceUnits::Kilometers, DistanceUnits::User>
      DistanceUnitsIterator;

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

} // namespace types
} // namespace qt
} // namespace scwx
