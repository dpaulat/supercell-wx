#include <scwx/qt/settings/unit_settings.hpp>
#include <scwx/qt/settings/settings_definitions.hpp>
#include <scwx/qt/types/unit_types.hpp>

#include <boost/algorithm/string.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ = "scwx::qt::settings::unit_settings";

class UnitSettings::Impl
{
public:
   explicit Impl()
   {
      std::string defaultAccumulationUnitsValue =
         types::GetAccumulationUnitsName(types::AccumulationUnits::Inches);
      std::string defaultEchoTopsUnitsValue =
         types::GetEchoTopsUnitsName(types::EchoTopsUnits::Kilofeet);
      std::string defaultOtherUnitsValue =
         types::GetOtherUnitsName(types::OtherUnits::Default);
      std::string defaultSpeedUnitsValue =
         types::GetSpeedUnitsName(types::SpeedUnits::Knots);
      std::string defaultDistanceUnitsValue =
         types::GetDistanceUnitsName(types::DistanceUnits::Kilometers);

      boost::to_lower(defaultAccumulationUnitsValue);
      boost::to_lower(defaultEchoTopsUnitsValue);
      boost::to_lower(defaultOtherUnitsValue);
      boost::to_lower(defaultSpeedUnitsValue);
      boost::to_lower(defaultDistanceUnitsValue);

      accumulationUnits_.SetDefault(defaultAccumulationUnitsValue);
      echoTopsUnits_.SetDefault(defaultEchoTopsUnitsValue);
      otherUnits_.SetDefault(defaultOtherUnitsValue);
      speedUnits_.SetDefault(defaultSpeedUnitsValue);
      distanceUnits_.SetDefault(defaultDistanceUnitsValue);

      accumulationUnits_.SetValidator(
         SCWX_SETTINGS_ENUM_VALIDATOR(types::AccumulationUnits,
                                      types::AccumulationUnitsIterator(),
                                      types::GetAccumulationUnitsName));
      echoTopsUnits_.SetValidator(
         SCWX_SETTINGS_ENUM_VALIDATOR(types::EchoTopsUnits,
                                      types::EchoTopsUnitsIterator(),
                                      types::GetEchoTopsUnitsName));
      otherUnits_.SetValidator(
         SCWX_SETTINGS_ENUM_VALIDATOR(types::OtherUnits,
                                      types::OtherUnitsIterator(),
                                      types::GetOtherUnitsName));
      speedUnits_.SetValidator(
         SCWX_SETTINGS_ENUM_VALIDATOR(types::SpeedUnits,
                                      types::SpeedUnitsIterator(),
                                      types::GetSpeedUnitsName));
      distanceUnits_.SetValidator(
         SCWX_SETTINGS_ENUM_VALIDATOR(types::DistanceUnits,
                                      types::DistanceUnitsIterator(),
                                      types::GetDistanceUnitsName));
   }

   ~Impl() {}

   SettingsVariable<std::string> accumulationUnits_ {"accumulation_units"};
   SettingsVariable<std::string> echoTopsUnits_ {"echo_tops_units"};
   SettingsVariable<std::string> otherUnits_ {"other_units"};
   SettingsVariable<std::string> speedUnits_ {"speed_units"};
   SettingsVariable<std::string> distanceUnits_ {"distance_units"};
};

UnitSettings::UnitSettings() :
    SettingsCategory("unit"), p(std::make_unique<Impl>())
{
   RegisterVariables({&p->accumulationUnits_,
                      &p->echoTopsUnits_,
                      &p->otherUnits_,
                      &p->speedUnits_,
                      &p->distanceUnits_});
   SetDefaults();
}
UnitSettings::~UnitSettings() = default;

UnitSettings::UnitSettings(UnitSettings&&) noexcept            = default;
UnitSettings& UnitSettings::operator=(UnitSettings&&) noexcept = default;

SettingsVariable<std::string>& UnitSettings::accumulation_units() const
{
   return p->accumulationUnits_;
}

SettingsVariable<std::string>& UnitSettings::echo_tops_units() const
{
   return p->echoTopsUnits_;
}

SettingsVariable<std::string>& UnitSettings::other_units() const
{
   return p->otherUnits_;
}

SettingsVariable<std::string>& UnitSettings::speed_units() const
{
   return p->speedUnits_;
}

SettingsVariable<std::string>& UnitSettings::distance_units() const
{
   return p->distanceUnits_;
}

UnitSettings& UnitSettings::Instance()
{
   static UnitSettings generalSettings_;
   return generalSettings_;
}

bool operator==(const UnitSettings& lhs, const UnitSettings& rhs)
{
   return (lhs.p->accumulationUnits_ == rhs.p->accumulationUnits_ &&
           lhs.p->echoTopsUnits_ == rhs.p->echoTopsUnits_ &&
           lhs.p->otherUnits_ == rhs.p->otherUnits_ &&
           lhs.p->speedUnits_ == rhs.p->speedUnits_ &&
           lhs.p->distanceUnits_ == rhs.p->distanceUnits_);
}

} // namespace settings
} // namespace qt
} // namespace scwx
