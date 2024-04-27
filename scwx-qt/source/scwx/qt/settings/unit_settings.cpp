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
      std::string defaultSpeedUnitsValue =
         types::GetSpeedUnitsName(types::SpeedUnits::Knots);

      boost::to_lower(defaultAccumulationUnitsValue);
      boost::to_lower(defaultEchoTopsUnitsValue);
      boost::to_lower(defaultSpeedUnitsValue);

      accumulationUnits_.SetDefault(defaultAccumulationUnitsValue);
      echoTopsUnits_.SetDefault(defaultEchoTopsUnitsValue);
      speedUnits_.SetDefault(defaultSpeedUnitsValue);

      accumulationUnits_.SetValidator(
         SCWX_SETTINGS_ENUM_VALIDATOR(types::AccumulationUnits,
                                      types::AccumulationUnitsIterator(),
                                      types::GetAccumulationUnitsName));
      echoTopsUnits_.SetValidator(
         SCWX_SETTINGS_ENUM_VALIDATOR(types::EchoTopsUnits,
                                      types::EchoTopsUnitsIterator(),
                                      types::GetEchoTopsUnitsName));
      speedUnits_.SetValidator(
         SCWX_SETTINGS_ENUM_VALIDATOR(types::SpeedUnits,
                                      types::SpeedUnitsIterator(),
                                      types::GetSpeedUnitsName));
   }

   ~Impl() {}

   SettingsVariable<std::string> accumulationUnits_ {"accumulation_units"};
   SettingsVariable<std::string> echoTopsUnits_ {"echo_tops_units"};
   SettingsVariable<std::string> speedUnits_ {"speed_units"};
};

UnitSettings::UnitSettings() :
    SettingsCategory("unit"), p(std::make_unique<Impl>())
{
   RegisterVariables(
      {&p->accumulationUnits_, &p->echoTopsUnits_, &p->speedUnits_});
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

SettingsVariable<std::string>& UnitSettings::speed_units() const
{
   return p->speedUnits_;
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
           lhs.p->speedUnits_ == rhs.p->speedUnits_);
}

} // namespace settings
} // namespace qt
} // namespace scwx
