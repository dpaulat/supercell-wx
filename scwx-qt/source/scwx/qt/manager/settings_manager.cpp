#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/util/json.hpp>
#include <scwx/util/logger.hpp>

#include <filesystem>
#include <fstream>

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

   std::string settingsPath {appDataPath + "/settings.json"};

   ReadSettings(settingsPath);
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

   return settingsJson;
}

static void GenerateDefaultSettings()
{
   logger_->info("Generating default settings");

   general_settings().SetDefaults();
   map_settings().SetDefaults();
   palette_settings().SetDefaults();
}

static bool LoadSettings(const boost::json::object& settingsJson)
{
   logger_->info("Loading settings");

   bool jsonDirty = false;

   jsonDirty |= !general_settings().ReadJson(settingsJson);
   jsonDirty |= !map_settings().ReadJson(settingsJson);
   jsonDirty |= !palette_settings().ReadJson(settingsJson);

   return jsonDirty;
}

} // namespace SettingsManager
} // namespace manager
} // namespace qt
} // namespace scwx
