#include <scwx/qt/settings/palette_settings.hpp>
#include <scwx/qt/settings/settings_variable.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ = "scwx::qt::settings::palette_settings";

static const std::vector<std::string> paletteNames_ = {
   // Level 2 / Common Products
   "BR",
   "BV",
   "SW",
   "ZDR",
   "PHI2",
   "CC",
   // Level 3 Products
   "DOD",
   "DSD",
   "ET",
   "OHP",
   "OHPIN",
   "PHI3",
   "SRV",
   "STP",
   "STPIN",
   "VIL",
   "???"};

static const std::string kDefaultKey     = "???";
static const std::string kDefaultPalette = "";

class PaletteSettingsImpl
{
public:
   explicit PaletteSettingsImpl()
   {
      std::for_each(paletteNames_.cbegin(),
                    paletteNames_.cend(),
                    [&](const std::string& name)
                    {
                       auto result = palette_.emplace(
                          name, SettingsVariable<std::string> {name});

                       SettingsVariable<std::string>& settingsVariable =
                          result.first->second;

                       settingsVariable.SetDefault(kDefaultPalette);

                       variables_.push_back(&settingsVariable);
                    });
   }

   ~PaletteSettingsImpl() {}

   std::unordered_map<std::string, SettingsVariable<std::string>> palette_;
   std::vector<SettingsVariableBase*>                             variables_;
};

PaletteSettings::PaletteSettings() :
    SettingsCategory("palette"), p(std::make_unique<PaletteSettingsImpl>())
{
   RegisterVariables(p->variables_);
   SetDefaults();

   p->variables_.clear();
}
PaletteSettings::~PaletteSettings() = default;

PaletteSettings::PaletteSettings(PaletteSettings&&) noexcept = default;
PaletteSettings&
PaletteSettings::operator=(PaletteSettings&&) noexcept = default;

std::string PaletteSettings::palette(const std::string& name) const
{
   auto palette = p->palette_.find(name);

   if (palette == p->palette_.cend())
   {
      palette = p->palette_.find(kDefaultKey);
   }

   if (palette == p->palette_.cend())
   {
      return kDefaultPalette;
   }

   return palette->second.GetValue();
}

bool operator==(const PaletteSettings& lhs, const PaletteSettings& rhs)
{
   return lhs.p->palette_ == rhs.p->palette_;
}

} // namespace settings
} // namespace qt
} // namespace scwx
