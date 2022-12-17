#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/util/json.hpp>
#include <scwx/util/logger.hpp>

#include <scwx/qt/settings/settings_container.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ = "scwx::qt::settings::general_settings";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class GeneralSettingsImpl
{
public:
   explicit GeneralSettingsImpl()
   {
      debugEnabled_.SetDefault(false);
      defaultRadarSite_.SetDefault("KLSX");
      fontSizes_.SetDefault({16});
      gridWidth_.SetDefault(1);
      gridHeight_.SetDefault(1);
      mapboxApiKey_.SetDefault("?");

      fontSizes_.SetElementMinimum(1);
      fontSizes_.SetElementMaximum(72);
      fontSizes_.SetValidator([](const std::vector<std::int64_t>& value)
                              { return !value.empty(); });
      gridWidth_.SetMinimum(1);
      gridWidth_.SetMaximum(2);
      gridHeight_.SetMinimum(1);
      gridHeight_.SetMaximum(2);
      mapboxApiKey_.SetValidator([](const std::string& value)
                                 { return !value.empty(); });

      RegisterVariables({&debugEnabled_,
                         &defaultRadarSite_,
                         &fontSizes_,
                         &gridWidth_,
                         &gridHeight_,
                         &mapboxApiKey_});

      SetDefaults();
   }

   ~GeneralSettingsImpl() {}

   void SetDefaults()
   {
      for (auto& variable : variables_)
      {
         variable->SetValueToDefault();
      }
   }

   SettingsVariable<bool>        debugEnabled_ {"debug_enabled"};
   SettingsVariable<std::string> defaultRadarSite_ {"default_radar_site"};
   SettingsContainer<std::vector<std::int64_t>> fontSizes_ {"font_sizes"};
   SettingsVariable<std::int64_t>               gridWidth_ {"grid_width"};
   SettingsVariable<std::int64_t>               gridHeight_ {"grid_height"};
   SettingsVariable<std::string> mapboxApiKey_ {"mapbox_api_key"};

   std::vector<SettingsVariableBase*> variables_;

   void
   RegisterVariables(std::initializer_list<SettingsVariableBase*> variables);
};

GeneralSettings::GeneralSettings() : p(std::make_unique<GeneralSettingsImpl>())
{
}
GeneralSettings::~GeneralSettings() = default;

GeneralSettings::GeneralSettings(GeneralSettings&&) noexcept = default;
GeneralSettings&
GeneralSettings::operator=(GeneralSettings&&) noexcept = default;

void GeneralSettingsImpl::RegisterVariables(
   std::initializer_list<SettingsVariableBase*> variables)
{
   variables_.insert(variables_.end(), variables);
}

bool GeneralSettings::debug_enabled() const
{
   return p->debugEnabled_.GetValue();
}

std::string GeneralSettings::default_radar_site() const
{
   return p->defaultRadarSite_.GetValue();
}

std::vector<std::int64_t> GeneralSettings::font_sizes() const
{
   return p->fontSizes_.GetValue();
}

std::int64_t GeneralSettings::grid_height() const
{
   return p->gridHeight_.GetValue();
}

std::int64_t GeneralSettings::grid_width() const
{
   return p->gridWidth_.GetValue();
}

std::string GeneralSettings::mapbox_api_key() const
{
   return p->mapboxApiKey_.GetValue();
}

boost::json::value GeneralSettings::ToJson() const
{
   boost::json::object json;

   for (auto& variable : p->variables_)
   {
      variable->WriteValue(json);
   }

   return json;
}

std::shared_ptr<GeneralSettings> GeneralSettings::Create()
{
   std::shared_ptr<GeneralSettings> generalSettings =
      std::make_shared<GeneralSettings>();

   return generalSettings;
}

std::shared_ptr<GeneralSettings>
GeneralSettings::Load(const boost::json::value* json, bool& jsonDirty)
{
   std::shared_ptr<GeneralSettings> generalSettings =
      std::make_shared<GeneralSettings>();

   if (json != nullptr && json->is_object())
   {
      for (auto& variable : generalSettings->p->variables_)
      {
         jsonDirty |= !variable->ReadValue(json->as_object());
      }
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

bool operator==(const GeneralSettings& lhs, const GeneralSettings& rhs)
{
   return (lhs.p->debugEnabled_ == rhs.p->debugEnabled_ &&
           lhs.p->defaultRadarSite_ == rhs.p->defaultRadarSite_ &&
           lhs.p->fontSizes_ == rhs.p->fontSizes_ &&
           lhs.p->gridWidth_ == rhs.p->gridWidth_ &&
           lhs.p->gridHeight_ == rhs.p->gridHeight_ &&
           lhs.p->mapboxApiKey_ == rhs.p->mapboxApiKey_);
}

} // namespace settings
} // namespace qt
} // namespace scwx
