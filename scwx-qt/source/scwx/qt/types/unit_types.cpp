#include <scwx/qt/types/unit_types.hpp>
#include <scwx/util/enum.hpp>

#include <unordered_map>

#include <boost/algorithm/string.hpp>

namespace scwx
{
namespace qt
{
namespace types
{

static const std::unordered_map<AccumulationUnits, std::string>
   accumulationUnitsAbbreviation_ {{AccumulationUnits::Inches, "in"},
                                   {AccumulationUnits::Millimeters, "mm"},
                                   {AccumulationUnits::User, "?"},
                                   {AccumulationUnits::Unknown, "?"}};

static const std::unordered_map<AccumulationUnits, std::string>
   accumulationUnitsName_ {{AccumulationUnits::Inches, "Inches"},
                           {AccumulationUnits::Millimeters, "Millimeters"},
                           {AccumulationUnits::User, "User-defined"},
                           {AccumulationUnits::Unknown, "?"}};

static const std::unordered_map<EchoTopsUnits, std::string>
   echoTopsUnitsAbbreviation_ {{EchoTopsUnits::Kilofeet, "kft"},
                               {EchoTopsUnits::Kilometers, "km"},
                               {EchoTopsUnits::User, "?"},
                               {EchoTopsUnits::Unknown, "?"}};

static const std::unordered_map<EchoTopsUnits, std::string> echoTopsUnitsName_ {
   {EchoTopsUnits::Kilofeet, "Kilofeet"},
   {EchoTopsUnits::Kilometers, "Kilometers"},
   {EchoTopsUnits::User, "User-defined"},
   {EchoTopsUnits::Unknown, "?"}};

static const std::unordered_map<SpeedUnits, std::string>
   speedUnitsAbbreviation_ {{SpeedUnits::KilometersPerHour, "km/h"},
                            {SpeedUnits::Knots, "kts"},
                            {SpeedUnits::MilesPerHour, "mph"},
                            {SpeedUnits::MetersPerSecond, "m/s"},
                            {SpeedUnits::User, "?"},
                            {SpeedUnits::Unknown, "?"}};

static const std::unordered_map<SpeedUnits, std::string> speedUnitsName_ {
   {SpeedUnits::KilometersPerHour, "Kilometers per hour"},
   {SpeedUnits::Knots, "Knots"},
   {SpeedUnits::MilesPerHour, "Miles per hour"},
   {SpeedUnits::MetersPerSecond, "Meters per second"},
   {SpeedUnits::User, "User-defined"},
   {SpeedUnits::Unknown, "?"}};

SCWX_GET_ENUM(AccumulationUnits,
              GetAccumulationUnitsFromName,
              accumulationUnitsName_)
SCWX_GET_ENUM(EchoTopsUnits, GetEchoTopsUnitsFromName, echoTopsUnitsName_)
SCWX_GET_ENUM(SpeedUnits, GetSpeedUnitsFromName, speedUnitsName_)

const std::string& GetAccumulationUnitsAbbreviation(AccumulationUnits units)
{
   return accumulationUnitsAbbreviation_.at(units);
}

const std::string& GetAccumulationUnitsName(AccumulationUnits units)
{
   return accumulationUnitsName_.at(units);
}

const std::string& GetEchoTopsUnitsAbbreviation(EchoTopsUnits units)
{
   return echoTopsUnitsAbbreviation_.at(units);
}

const std::string& GetEchoTopsUnitsName(EchoTopsUnits units)
{
   return echoTopsUnitsName_.at(units);
}

const std::string& GetSpeedUnitsAbbreviation(SpeedUnits units)
{
   return speedUnitsAbbreviation_.at(units);
}

const std::string& GetSpeedUnitsName(SpeedUnits units)
{
   return speedUnitsName_.at(units);
}

} // namespace types
} // namespace qt
} // namespace scwx
