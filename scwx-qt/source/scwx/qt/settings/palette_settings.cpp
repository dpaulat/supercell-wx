#include <scwx/qt/settings/palette_settings.hpp>
#include <scwx/qt/settings/settings_variable.hpp>
#include <scwx/qt/util/color.hpp>

#include <boost/gil.hpp>
#include <fmt/format.h>
#include <re2/re2.h>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ = "scwx::qt::settings::palette_settings";

static const std::array<std::string, 18> kPaletteKeys_ {
   // Level 2 / Common Products
   "BR",
   "BV",
   "SW",
   "CC",
   "ZDR",
   "PHI2",
   // Level 3 Products
   "DOD",
   "DSD",
   "ET",
   "HC",
   "STP",
   "OHP",
   "STPIN",
   "OHPIN",
   "PHI3",
   "SRV",
   "VIL",
   "???"};

static const std::unordered_map<std::string, std::string> kDefaultPalettes_ {
   // Level 2 / Common Products
   {"BR", ":/res/palettes/wct/DR.pal"},
   {"BV", ":/res/palettes/wct/DV.pal"},
   {"SW", ":/res/palettes/wct/SW.pal"},
   {"ZDR", ":/res/palettes/wct/ZDR.pal"},
   {"PHI2", ":/res/palettes/wct/KDP2.pal"},
   {"CC", ":/res/palettes/wct/CC.pal"},
   // Level 3 Products
   {"DOD", ":/res/palettes/wct/DOD_DSD.pal"},
   {"DSD", ":/res/palettes/wct/DOD_DSD.pal"},
   {"ET", ":/res/palettes/wct/ET.pal"},
   {"HC", ":/res/palettes/wct/HC.pal"},
   {"OHP", ":/res/palettes/wct/OHP.pal"},
   {"OHPIN", ""},
   {"PHI3", ":/res/palettes/wct/KDP.pal"},
   {"SRV", ":/res/palettes/wct/SRV.pal"},
   {"STP", ":/res/palettes/wct/STP.pal"},
   {"STPIN", ""},
   {"VIL", ":/res/palettes/wct/VIL.pal"},
   {"???", ":/res/palettes/wct/Default16.pal"}};

static const std::map<
   awips::Phenomenon,
   std::pair<boost::gil::rgba8_pixel_t, boost::gil::rgba8_pixel_t>>
   kAlertColors_ {
      {awips::Phenomenon::Marine, {{255, 127, 0, 255}, {127, 63, 0, 255}}},
      {awips::Phenomenon::FlashFlood, {{0, 255, 0, 255}, {0, 127, 0, 255}}},
      {awips::Phenomenon::SevereThunderstorm,
       {{255, 255, 0, 255}, {127, 127, 0, 255}}},
      {awips::Phenomenon::SnowSquall, {{0, 255, 255, 255}, {0, 127, 127, 255}}},
      {awips::Phenomenon::Tornado, {{255, 0, 0, 255}, {127, 0, 0, 255}}}};

static const std::string       kDefaultKey_ {"???"};
static const awips::Phenomenon kDefaultPhenomenon_ {awips::Phenomenon::Marine};

class PaletteSettings::Impl
{
public:
   explicit Impl()
   {
      for (const auto& name : kPaletteKeys_)
      {
         const std::string& defaultValue = kDefaultPalettes_.at(name);

         auto result =
            palette_.emplace(name, SettingsVariable<std::string> {name});

         SettingsVariable<std::string>& settingsVariable = result.first->second;

         settingsVariable.SetDefault(defaultValue);

         variables_.push_back(&settingsVariable);
      };

      for (auto& alert : kAlertColors_)
      {
         std::string phenomenonCode = awips::GetPhenomenonCode(alert.first);
         std::string activeName     = fmt::format("{}-active", phenomenonCode);
         std::string inactiveName = fmt::format("{}-inactive", phenomenonCode);

         auto activeResult = activeAlertColor_.emplace(
            alert.first, SettingsVariable<std::string> {activeName});
         auto inactiveResult = inactiveAlertColor_.emplace(
            alert.first, SettingsVariable<std::string> {inactiveName});

         SettingsVariable<std::string>& activeVariable =
            activeResult.first->second;
         SettingsVariable<std::string>& inactiveVariable =
            inactiveResult.first->second;

         activeVariable.SetDefault(
            util::color::ToArgbString(alert.second.first));
         inactiveVariable.SetDefault(
            util::color::ToArgbString(alert.second.second));

         activeVariable.SetValidator(&ValidateColor);
         inactiveVariable.SetValidator(&ValidateColor);

         variables_.push_back(&activeVariable);
         variables_.push_back(&inactiveVariable);
      }
   }

   ~Impl() {}

   static bool ValidateColor(const std::string& value);

   std::unordered_map<std::string, SettingsVariable<std::string>> palette_ {};
   std::unordered_map<awips::Phenomenon, SettingsVariable<std::string>>
      activeAlertColor_ {};
   std::unordered_map<awips::Phenomenon, SettingsVariable<std::string>>
                                      inactiveAlertColor_ {};
   std::vector<SettingsVariableBase*> variables_ {};
};

bool PaletteSettings::Impl::ValidateColor(const std::string& value)
{
   static constexpr LazyRE2 re = {"#[0-9A-Fa-f]{8}"};
   return RE2::FullMatch(value, *re);
}

PaletteSettings::PaletteSettings() :
    SettingsCategory("palette"), p(std::make_unique<Impl>())
{
   RegisterVariables(p->variables_);
   SetDefaults();

   p->variables_.clear();
}
PaletteSettings::~PaletteSettings() = default;

PaletteSettings::PaletteSettings(PaletteSettings&&) noexcept = default;
PaletteSettings&
PaletteSettings::operator=(PaletteSettings&&) noexcept = default;

SettingsVariable<std::string>&
PaletteSettings::palette(const std::string& name) const
{
   auto palette = p->palette_.find(name);

   if (palette == p->palette_.cend())
   {
      palette = p->palette_.find(kDefaultKey_);
   }

   return palette->second;
}

SettingsVariable<std::string>&
PaletteSettings::alert_color(awips::Phenomenon phenomenon, bool active) const
{
   if (active)
   {
      auto alert = p->activeAlertColor_.find(phenomenon);
      if (alert == p->activeAlertColor_.cend())
      {
         alert = p->activeAlertColor_.find(kDefaultPhenomenon_);
      }
      return alert->second;
   }
   else
   {
      auto alert = p->inactiveAlertColor_.find(phenomenon);
      if (alert == p->inactiveAlertColor_.cend())
      {
         alert = p->inactiveAlertColor_.find(kDefaultPhenomenon_);
      }
      return alert->second;
   }
}

const std::vector<awips::Phenomenon>& PaletteSettings::alert_phenomena()
{
   static const std::vector<awips::Phenomenon> kAlertPhenomena_ {
      awips::Phenomenon::Marine,
      awips::Phenomenon::FlashFlood,
      awips::Phenomenon::SevereThunderstorm,
      awips::Phenomenon::SnowSquall,
      awips::Phenomenon::Tornado};

   return kAlertPhenomena_;
}

PaletteSettings& PaletteSettings::Instance()
{
   static PaletteSettings paletteSettings_;
   return paletteSettings_;
}

bool operator==(const PaletteSettings& lhs, const PaletteSettings& rhs)
{
   return (lhs.p->palette_ == rhs.p->palette_ &&
           lhs.p->activeAlertColor_ == rhs.p->activeAlertColor_ &&
           lhs.p->inactiveAlertColor_ == rhs.p->inactiveAlertColor_);
}

} // namespace settings
} // namespace qt
} // namespace scwx
