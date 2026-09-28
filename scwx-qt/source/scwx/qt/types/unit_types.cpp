#include <scwx/qt/types/unit_types.hpp>
#include <scwx/util/enum.hpp>

#include <unordered_map>

#include <boost/algorithm/string.hpp>
#include <units/pressure.h>
#include <units/temperature.h>
#include <units/velocity.h>

namespace scwx::qt::types
{

static const std::unordered_map<AccumulationUnits, std::string>
   accumulationUnitsAbbreviation_ {{AccumulationUnits::Inches, "in"},
                                   {AccumulationUnits::Millimeters, "mm"},
                                   {AccumulationUnits::User, ""},
                                   {AccumulationUnits::Unknown, ""}};

static const std::unordered_map<AccumulationUnits, std::string>
   accumulationUnitsName_ {{AccumulationUnits::Inches, "Inches"},
                           {AccumulationUnits::Millimeters, "Millimeters"},
                           {AccumulationUnits::User, "User-defined"},
                           {AccumulationUnits::Unknown, "?"}};

static constexpr auto accumulationUnitsBase_ = units::millimeters<float> {1.0f};
static const std::unordered_map<AccumulationUnits, float>
   accumulationUnitsScale_ {
      {AccumulationUnits::Inches,
       (accumulationUnitsBase_ / units::inches<float> {1.0f})},
      {AccumulationUnits::Millimeters,
       (accumulationUnitsBase_ / units::millimeters<float> {1.0f})},
      {AccumulationUnits::User, 1.0f},
      {AccumulationUnits::Unknown, 1.0f}};

static const std::unordered_map<EchoTopsUnits, std::string>
   echoTopsUnitsAbbreviation_ {{EchoTopsUnits::Kilofeet, "kft"},
                               {EchoTopsUnits::Kilometers, "km"},
                               {EchoTopsUnits::User, ""},
                               {EchoTopsUnits::Unknown, ""}};

static const std::unordered_map<EchoTopsUnits, std::string> echoTopsUnitsName_ {
   {EchoTopsUnits::Kilofeet, "Kilofeet"},
   {EchoTopsUnits::Kilometers, "Kilometers"},
   {EchoTopsUnits::User, "User-defined"},
   {EchoTopsUnits::Unknown, "?"}};

static constexpr auto echoTopsUnitsBase_ = units::kilometers<float> {1.0f};
static const std::unordered_map<EchoTopsUnits, float> echoTopsUnitsScale_ {
   {EchoTopsUnits::Kilofeet,
    (echoTopsUnitsBase_ / units::feet<float> {1000.0f})},
   {EchoTopsUnits::Kilometers,
    (echoTopsUnitsBase_ / units::kilometers<float> {1.0f})},
   {EchoTopsUnits::User, 1.0f},
   {EchoTopsUnits::Unknown, 1.0f}};

static const std::unordered_map<OtherUnits, std::string> otherUnitsName_ {
   {OtherUnits::Default, "Default"},
   {OtherUnits::User, "User-defined"},
   {OtherUnits::Unknown, "?"}};

static const std::unordered_map<SpeedUnits, std::string>
   speedUnitsAbbreviation_ {{SpeedUnits::KilometersPerHour, "km/h"},
                            {SpeedUnits::Knots, "kts"},
                            {SpeedUnits::MilesPerHour, "mph"},
                            {SpeedUnits::MetersPerSecond, "m/s"},
                            {SpeedUnits::User, ""},
                            {SpeedUnits::Unknown, ""}};

static const std::unordered_map<SpeedUnits, std::string> speedUnitsName_ {
   {SpeedUnits::KilometersPerHour, "Kilometers per hour"},
   {SpeedUnits::Knots, "Knots"},
   {SpeedUnits::MilesPerHour, "Miles per hour"},
   {SpeedUnits::MetersPerSecond, "Meters per second"},
   {SpeedUnits::User, "User-defined"},
   {SpeedUnits::Unknown, "?"}};

static constexpr auto speedUnitsBase_ = units::meters_per_second<float> {1.0f};
static const std::unordered_map<SpeedUnits, float> speedUnitsScale_ {
   {SpeedUnits::KilometersPerHour,
    (speedUnitsBase_ / units::kilometers_per_hour<float> {1.0f})},
   {SpeedUnits::Knots, (speedUnitsBase_ / units::knots<float> {1.0f})},
   {SpeedUnits::MilesPerHour,
    (speedUnitsBase_ / units::miles_per_hour<float> {1.0f})},
   {SpeedUnits::MetersPerSecond,
    (speedUnitsBase_ / units::meters_per_second<float> {1.0f})},
   {SpeedUnits::User, 1.0f},
   {SpeedUnits::Unknown, 1.0f}};

static const std::unordered_map<DistanceUnits, std::string>
   distanceUnitsAbbreviation_ {{DistanceUnits::Kilometers, "km"},
                               {DistanceUnits::Miles, "mi"},
                               {DistanceUnits::User, ""},
                               {DistanceUnits::Unknown, ""}};

static const std::unordered_map<DistanceUnits, std::string> distanceUnitsName_ {
   {DistanceUnits::Kilometers, "Kilometers"},
   {DistanceUnits::Miles, "Miles"},
   {DistanceUnits::User, "User-defined"},
   {DistanceUnits::Unknown, "?"}};

static constexpr auto distanceUnitsBase_ = units::kilometers<double> {1.0f};
static const std::unordered_map<DistanceUnits, double> distanceUnitsScale_ {
   {DistanceUnits::Kilometers,
    (distanceUnitsBase_ / units::kilometers<double> {1.0f})},
   {DistanceUnits::Miles, (distanceUnitsBase_ / units::miles<double> {1.0f})},
   {DistanceUnits::User, 1.0f},
   {DistanceUnits::Unknown, 1.0f}};

static const std::unordered_map<TemperatureUnits, std::string>
   temperatureUnitsAbbreviation_ {{TemperatureUnits::Celsius, "°C"},
                                  {TemperatureUnits::Fahrenheit, "°F"},
                                  {TemperatureUnits::Kelvin, "K"},
                                  {TemperatureUnits::Unknown, ""}};

static const std::unordered_map<TemperatureUnits, std::string>
   temperatureUnitsName_ {{TemperatureUnits::Celsius, "Celsius"},
                          {TemperatureUnits::Fahrenheit, "Fahrenheit"},
                          {TemperatureUnits::Kelvin, "Kelvin"},
                          {TemperatureUnits::Unknown, "?"}};

static const std::unordered_map<PressureUnits, std::string>
   pressureUnitsAbbreviation_ {{PressureUnits::InchesOfMercury, "inHg"},
                               {PressureUnits::Hectopascals, "hPa"},
                               {PressureUnits::MillimetersOfMercury, "mmHg"},
                               {PressureUnits::Unknown, ""}};

static const std::unordered_map<PressureUnits, std::string> pressureUnitsName_ {
   {PressureUnits::InchesOfMercury, "Inches of Mercury"},
   {PressureUnits::Hectopascals, "Hectopascals"},
   {PressureUnits::MillimetersOfMercury, "Millimeters of Mercury"},
   {PressureUnits::Unknown, "?"}};

// The units library has no built-in inHg literal -- computed from mmHg via
// the exact defined relationship 1 inch = 25.4 mm (both inHg and mmHg are
// simply "height of a mercury column", so that's also exactly the inHg:
// mmHg ratio).
static constexpr auto pressureUnitsBase_ =
   units::pressure::pascals<float> {1.0f};
static const std::unordered_map<PressureUnits, float> pressureUnitsScale_ {
   {PressureUnits::InchesOfMercury,
    (pressureUnitsBase_ / units::pressure::mmHg<float> {25.4f})},
   {PressureUnits::Hectopascals,
    (pressureUnitsBase_ / units::pressure::hectopascals<float> {1.0f})},
   {PressureUnits::MillimetersOfMercury,
    (pressureUnitsBase_ / units::pressure::mmHg<float> {1.0f})},
   {PressureUnits::Unknown, 1.0f}};

static const std::unordered_map<RadarBeamHeightReference, std::string>
   radarBeamHeightReferenceAbbreviation_ {
      {RadarBeamHeightReference::AboveRadarLevel, "ARL"},
      {RadarBeamHeightReference::MeanSeaLevel, "MSL"},
      {RadarBeamHeightReference::Unknown, ""}};

static const std::unordered_map<RadarBeamHeightReference, std::string>
   radarBeamHeightReferenceName_ {
      {RadarBeamHeightReference::AboveRadarLevel, "Above Radar Level"},
      {RadarBeamHeightReference::MeanSeaLevel, "Mean Sea Level"},
      {RadarBeamHeightReference::Unknown, "?"}};

SCWX_GET_ENUM(AccumulationUnits,
              GetAccumulationUnitsFromName,
              accumulationUnitsName_)
SCWX_GET_ENUM(EchoTopsUnits, GetEchoTopsUnitsFromName, echoTopsUnitsName_)
SCWX_GET_ENUM(OtherUnits, GetOtherUnitsFromName, otherUnitsName_)
SCWX_GET_ENUM(SpeedUnits, GetSpeedUnitsFromName, speedUnitsName_)
SCWX_GET_ENUM(DistanceUnits, GetDistanceUnitsFromName, distanceUnitsName_)
SCWX_GET_ENUM(TemperatureUnits,
              GetTemperatureUnitsFromName,
              temperatureUnitsName_)
SCWX_GET_ENUM(PressureUnits, GetPressureUnitsFromName, pressureUnitsName_)
SCWX_GET_ENUM(RadarBeamHeightReference,
              GetRadarBeamHeightReferenceFromName,
              radarBeamHeightReferenceName_)

const std::string& GetAccumulationUnitsAbbreviation(AccumulationUnits units)
{ return accumulationUnitsAbbreviation_.at(units); }

const std::string& GetAccumulationUnitsName(AccumulationUnits units)
{ return accumulationUnitsName_.at(units); }

float GetAccumulationUnitsScale(AccumulationUnits units)
{ return accumulationUnitsScale_.at(units); }

const std::string& GetEchoTopsUnitsAbbreviation(EchoTopsUnits units)
{ return echoTopsUnitsAbbreviation_.at(units); }

const std::string& GetEchoTopsUnitsName(EchoTopsUnits units)
{ return echoTopsUnitsName_.at(units); }

float GetEchoTopsUnitsScale(EchoTopsUnits units)
{ return echoTopsUnitsScale_.at(units); }

const std::string& GetOtherUnitsName(OtherUnits units)
{ return otherUnitsName_.at(units); }

const std::string& GetSpeedUnitsAbbreviation(SpeedUnits units)
{ return speedUnitsAbbreviation_.at(units); }

const std::string& GetSpeedUnitsName(SpeedUnits units)
{ return speedUnitsName_.at(units); }

float GetSpeedUnitsScale(SpeedUnits units)
{ return speedUnitsScale_.at(units); }

const std::string& GetDistanceUnitsAbbreviation(DistanceUnits units)
{ return distanceUnitsAbbreviation_.at(units); }

const std::string& GetDistanceUnitsName(DistanceUnits units)
{ return distanceUnitsName_.at(units); }

double GetDistanceUnitsScale(DistanceUnits units)
{ return distanceUnitsScale_.at(units); }

const std::string& GetTemperatureUnitsAbbreviation(TemperatureUnits units)
{ return temperatureUnitsAbbreviation_.at(units); }

const std::string& GetTemperatureUnitsName(TemperatureUnits units)
{ return temperatureUnitsName_.at(units); }

float ConvertTemperatureFromKelvin(float kelvinValue, TemperatureUnits to)
{
   const units::temperature::kelvin<float> kelvin {kelvinValue};

   switch (to)
   {
   case TemperatureUnits::Celsius:
      return units::temperature::celsius<float> {kelvin}.value();
   case TemperatureUnits::Fahrenheit:
      return units::temperature::fahrenheit<float> {kelvin}.value();
   case TemperatureUnits::Kelvin:
   case TemperatureUnits::Unknown:
   default:
      return kelvinValue;
   }
}

const std::string& GetPressureUnitsAbbreviation(PressureUnits units)
{ return pressureUnitsAbbreviation_.at(units); }

const std::string& GetPressureUnitsName(PressureUnits units)
{ return pressureUnitsName_.at(units); }

float GetPressureUnitsScale(PressureUnits units)
{ return pressureUnitsScale_.at(units); }

const std::string&
GetRadarBeamHeightReferenceAbbreviation(RadarBeamHeightReference reference)
{ return radarBeamHeightReferenceAbbreviation_.at(reference); }

const std::string&
GetRadarBeamHeightReferenceName(RadarBeamHeightReference reference)
{ return radarBeamHeightReferenceName_.at(reference); }

} // namespace scwx::qt::types
