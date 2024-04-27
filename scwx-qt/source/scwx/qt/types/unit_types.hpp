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

const std::string& GetAccumulationUnitsAbbreviation(AccumulationUnits units);
const std::string& GetAccumulationUnitsName(AccumulationUnits units);
AccumulationUnits  GetAccumulationUnitsFromName(const std::string& name);

const std::string& GetEchoTopsUnitsAbbreviation(EchoTopsUnits units);
const std::string& GetEchoTopsUnitsName(EchoTopsUnits units);
EchoTopsUnits      GetEchoTopsUnitsFromName(const std::string& name);

const std::string& GetSpeedUnitsAbbreviation(SpeedUnits units);
const std::string& GetSpeedUnitsName(SpeedUnits units);
SpeedUnits         GetSpeedUnitsFromName(const std::string& name);

} // namespace types
} // namespace qt
} // namespace scwx
