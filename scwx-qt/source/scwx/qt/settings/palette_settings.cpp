#include <scwx/qt/settings/palette_settings.hpp>
#include <scwx/qt/util/json.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ = "scwx::qt::settings::palette_settings";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

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

static const std::string DEFAULT_KEY     = "Default";
static const std::string DEFAULT_PALETTE = "";

class PaletteSettingsImpl
{
public:
   explicit PaletteSettingsImpl() {}

   ~PaletteSettingsImpl() {}

   void SetDefaults()
   {
      std::for_each(paletteNames_.cbegin(),
                    paletteNames_.cend(),
                    [&](const std::string& name)
                    { palette_[name] = DEFAULT_PALETTE; });
   }

   std::unordered_map<std::string, std::string> palette_;
};

PaletteSettings::PaletteSettings() : p(std::make_unique<PaletteSettingsImpl>())
{
}
PaletteSettings::~PaletteSettings() = default;

PaletteSettings::PaletteSettings(PaletteSettings&&) noexcept = default;
PaletteSettings&
PaletteSettings::operator=(PaletteSettings&&) noexcept = default;

const std::string& PaletteSettings::palette(const std::string& name) const
{
   auto palette = p->palette_.find(name);

   if (palette == p->palette_.cend())
   {
      palette = p->palette_.find("Default");
   }

   if (palette == p->palette_.cend())
   {
      return DEFAULT_PALETTE;
   }

   return palette->second;
}

boost::json::value PaletteSettings::ToJson() const
{
   boost::json::object json;

   std::for_each(paletteNames_.cbegin(),
                 paletteNames_.cend(),
                 [&](const std::string& name)
                 { json[name] = p->palette_[name]; });

   return json;
}

std::shared_ptr<PaletteSettings> PaletteSettings::Create()
{
   std::shared_ptr<PaletteSettings> generalSettings =
      std::make_shared<PaletteSettings>();

   generalSettings->p->SetDefaults();

   return generalSettings;
}

std::shared_ptr<PaletteSettings>
PaletteSettings::Load(const boost::json::value* json, bool& jsonDirty)
{
   std::shared_ptr<PaletteSettings> generalSettings =
      std::make_shared<PaletteSettings>();

   if (json != nullptr && json->is_object())
   {
      std::for_each(paletteNames_.cbegin(),
                    paletteNames_.cend(),
                    [&](const std::string& name)
                    {
                       jsonDirty |= !util::json::FromJsonString(
                          json->as_object(),
                          name,
                          generalSettings->p->palette_[name],
                          DEFAULT_PALETTE);
                    });
   }
   else
   {
      if (json == nullptr)
      {
         logger_->warn("Key is not present, resetting to defaults");
      }
      else if (!json->is_object())
      {
         logger_->warn("Invalid json, resetting to defaults");
      }

      generalSettings->p->SetDefaults();
      jsonDirty = true;
   }

   return generalSettings;
}

bool operator==(const PaletteSettings& lhs, const PaletteSettings& rhs)
{
   return lhs.p->palette_ == rhs.p->palette_;
}

} // namespace settings
} // namespace qt
} // namespace scwx
