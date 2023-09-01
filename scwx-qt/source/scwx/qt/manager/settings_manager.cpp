#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/map/map_provider.hpp>
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
namespace SettingsManager
{

static const std::string logPrefix_ = "scwx::qt::manager::settings_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static boost::json::value ConvertSettingsToJson();
static void               GenerateDefaultSettings();
static bool               LoadSettings(const boost::json::object& settingsJson);
static void               ValidateSettings();

static bool        initialized_ {false};
static std::string settingsPath_ {};

void Initialize()
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

   settingsPath_ = appDataPath + "/settings.json";
   initialized_  = true;

   ReadSettings(settingsPath_);
   ValidateSettings();
}

void ReadSettings(const std::string& settingsPath)
{
   boost::json::value settingsJson = nullptr;

   if (std::filesystem::exists(settingsPath))
   {
      settingsJson = util::json::ReadJsonFile(settingsPath);
   }

   if (settingsJson == nullptr || !settingsJson.is_object())
   {
      GenerateDefaultSettings();
      settingsJson = ConvertSettingsToJson();
      util::json::WriteJsonFile(settingsPath, settingsJson);
   }
   else
   {
      bool jsonDirty = LoadSettings(settingsJson.as_object());

      if (jsonDirty)
      {
         settingsJson = ConvertSettingsToJson();
         util::json::WriteJsonFile(settingsPath, settingsJson);
      }
   };
}

void SaveSettings()
{
   if (initialized_)
   {
      logger_->info("Saving settings");

      boost::json::value settingsJson = ConvertSettingsToJson();
      util::json::WriteJsonFile(settingsPath_, settingsJson);
   }
}

void Shutdown()
{
   bool dataChanged = false;

   dataChanged |= general_settings().Shutdown();
   dataChanged |= map_settings().Shutdown();
   dataChanged |= settings::UiSettings::Instance().Shutdown();

   if (dataChanged)
   {
      SaveSettings();
   }
}

settings::GeneralSettings& general_settings()
{
   static settings::GeneralSettings generalSettings_;
   return generalSettings_;
}

settings::MapSettings& map_settings()
{
   static settings::MapSettings mapSettings_;
   return mapSettings_;
}

settings::PaletteSettings& palette_settings()
{
   static settings::PaletteSettings paletteSettings_;
   return paletteSettings_;
}

static boost::json::value ConvertSettingsToJson()
{
   boost::json::object settingsJson;

   general_settings().WriteJson(settingsJson);
   map_settings().WriteJson(settingsJson);
   palette_settings().WriteJson(settingsJson);
   settings::TextSettings::Instance().WriteJson(settingsJson);
   settings::UiSettings::Instance().WriteJson(settingsJson);

   return settingsJson;
}

static void GenerateDefaultSettings()
{
   logger_->info("Generating default settings");

   general_settings().SetDefaults();
   map_settings().SetDefaults();
   palette_settings().SetDefaults();
   settings::TextSettings::Instance().SetDefaults();
   settings::UiSettings::Instance().SetDefaults();
}

static bool LoadSettings(const boost::json::object& settingsJson)
{
   logger_->info("Loading settings");

   bool jsonDirty = false;

   jsonDirty |= !general_settings().ReadJson(settingsJson);
   jsonDirty |= !map_settings().ReadJson(settingsJson);
   jsonDirty |= !palette_settings().ReadJson(settingsJson);
   jsonDirty |= !settings::TextSettings::Instance().ReadJson(settingsJson);
   jsonDirty |= !settings::UiSettings::Instance().ReadJson(settingsJson);

   return jsonDirty;
}

static void ValidateSettings()
{
   logger_->debug("Validating settings");

   bool settingsChanged = false;

   auto& generalSettings = general_settings();

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
      SaveSettings();
   }
}

} // namespace SettingsManager
} // namespace manager
} // namespace qt
} // namespace scwx
