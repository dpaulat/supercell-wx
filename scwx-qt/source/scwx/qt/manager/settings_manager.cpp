#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/map/map_provider.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/settings/map_settings.hpp>
#include <scwx/qt/settings/palette_settings.hpp>
#include <scwx/qt/settings/text_settings.hpp>
#include <scwx/qt/settings/ui_settings.hpp>
#include <scwx/qt/util/json.hpp>
#include <scwx/util/logger.hpp>

#include <filesystem>
#include <fstream>

#include <boost/algorithm/string.hpp>
#include <QDir>
#include <QStandardPaths>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::settings_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class SettingsManager::Impl
{
public:
   explicit Impl(SettingsManager* self) : self_ {self} {}
   ~Impl() = default;

   void ValidateSettings();

   static boost::json::value ConvertSettingsToJson();
   static void               GenerateDefaultSettings();
   static bool LoadSettings(const boost::json::object& settingsJson);

   SettingsManager* self_;

   bool        initialized_ {false};
   std::string settingsPath_ {};
};

SettingsManager::SettingsManager() : p(std::make_unique<Impl>(this)) {}

SettingsManager::~SettingsManager() {};

void SettingsManager::Initialize()
{
   std::string appDataPath {
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
         .toStdString()};

   if (!std::filesystem::exists(appDataPath))
   {
      if (!std::filesystem::create_directories(appDataPath))
      {
         logger_->error("Unable to create application data directory: \"{}\"",
                        appDataPath);
      }
   }

   p->settingsPath_ = appDataPath + "/settings.json";
   p->initialized_  = true;

   ReadSettings(p->settingsPath_);
   p->ValidateSettings();
}

void SettingsManager::ReadSettings(const std::string& settingsPath)
{
   boost::json::value settingsJson = nullptr;

   if (std::filesystem::exists(settingsPath))
   {
      settingsJson = util::json::ReadJsonFile(settingsPath);
   }

   if (settingsJson == nullptr || !settingsJson.is_object())
   {
      Impl::GenerateDefaultSettings();
      settingsJson = Impl::ConvertSettingsToJson();
      util::json::WriteJsonFile(settingsPath, settingsJson);
   }
   else
   {
      bool jsonDirty = Impl::LoadSettings(settingsJson.as_object());

      if (jsonDirty)
      {
         settingsJson = Impl::ConvertSettingsToJson();
         util::json::WriteJsonFile(settingsPath, settingsJson);
      }
   };
}

void SettingsManager::SaveSettings()
{
   if (p->initialized_)
   {
      logger_->info("Saving settings");

      boost::json::value settingsJson = Impl::ConvertSettingsToJson();
      util::json::WriteJsonFile(p->settingsPath_, settingsJson);
   }
}

void SettingsManager::Shutdown()
{
   bool dataChanged = false;

   dataChanged |= settings::GeneralSettings::Instance().Shutdown();
   dataChanged |= settings::MapSettings::Instance().Shutdown();
   dataChanged |= settings::UiSettings::Instance().Shutdown();

   if (dataChanged)
   {
      SaveSettings();
   }
}

boost::json::value SettingsManager::Impl::ConvertSettingsToJson()
{
   boost::json::object settingsJson;

   settings::GeneralSettings::Instance().WriteJson(settingsJson);
   settings::MapSettings::Instance().WriteJson(settingsJson);
   settings::PaletteSettings::Instance().WriteJson(settingsJson);
   settings::TextSettings::Instance().WriteJson(settingsJson);
   settings::UiSettings::Instance().WriteJson(settingsJson);

   return settingsJson;
}

void SettingsManager::Impl::GenerateDefaultSettings()
{
   logger_->info("Generating default settings");

   settings::GeneralSettings::Instance().SetDefaults();
   settings::MapSettings::Instance().SetDefaults();
   settings::PaletteSettings::Instance().SetDefaults();
   settings::TextSettings::Instance().SetDefaults();
   settings::UiSettings::Instance().SetDefaults();
}

bool SettingsManager::Impl::LoadSettings(
   const boost::json::object& settingsJson)
{
   logger_->info("Loading settings");

   bool jsonDirty = false;

   jsonDirty |= !settings::GeneralSettings::Instance().ReadJson(settingsJson);
   jsonDirty |= !settings::MapSettings::Instance().ReadJson(settingsJson);
   jsonDirty |= !settings::PaletteSettings::Instance().ReadJson(settingsJson);
   jsonDirty |= !settings::TextSettings::Instance().ReadJson(settingsJson);
   jsonDirty |= !settings::UiSettings::Instance().ReadJson(settingsJson);

   return jsonDirty;
}

void SettingsManager::Impl::ValidateSettings()
{
   logger_->debug("Validating settings");

   bool settingsChanged = false;

   auto& generalSettings = settings::GeneralSettings::Instance();

   // Validate map provider
   std::string mapProviderName = generalSettings.map_provider().GetValue();
   std::string mapboxApiKey    = generalSettings.mapbox_api_key().GetValue();
   std::string maptilerApiKey  = generalSettings.maptiler_api_key().GetValue();

   map::MapProvider mapProvider = map::GetMapProvider(mapProviderName);
   std::string      mapApiKey   = map::GetMapProviderApiKey(mapProvider);

   if (mapApiKey == "?")
   {
      for (map::MapProvider newProvider : map::MapProviderIterator())
      {
         if (mapProvider != newProvider &&
             map::GetMapProviderApiKey(newProvider).size() > 1)
         {
            logger_->info(
               "Setting Map Provider to {} based on API key settings",
               map::GetMapProviderName(newProvider));

            std::string newProviderName {GetMapProviderName(newProvider)};
            boost::to_lower(newProviderName);
            generalSettings.map_provider().SetValue(newProviderName);
            settingsChanged = true;
         }
      }
   }

   if (settingsChanged)
   {
      self_->SaveSettings();
   }
}

SettingsManager& SettingsManager::Instance()
{
   static SettingsManager instance_ {};
   return instance_;
}

} // namespace manager
} // namespace qt
} // namespace scwx
