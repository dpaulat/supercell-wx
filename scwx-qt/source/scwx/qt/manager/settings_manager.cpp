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

static const std::string kGeneralKey = "general";
static const std::string kMapKey     = "maps";
static const std::string kPaletteKey = "palette";

static std::shared_ptr<settings::GeneralSettings> generalSettings_ = nullptr;
static std::shared_ptr<settings::MapSettings>     mapSettings_     = nullptr;
static std::shared_ptr<settings::PaletteSettings> paletteSettings_ = nullptr;

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

std::shared_ptr<settings::GeneralSettings> general_settings()
{
   return generalSettings_;
}

std::shared_ptr<settings::MapSettings> map_settings()
{
   return mapSettings_;
}

std::shared_ptr<settings::PaletteSettings> palette_settings()
{
   return paletteSettings_;
}

static boost::json::value ConvertSettingsToJson()
{
   boost::json::object settingsJson;

   generalSettings_->WriteJson(settingsJson);
   settingsJson[kMapKey]     = mapSettings_->ToJson();
   settingsJson[kPaletteKey] = paletteSettings_->ToJson();

   return settingsJson;
}

static void GenerateDefaultSettings()
{
   logger_->info("Generating default settings");

   generalSettings_ = std::make_shared<settings::GeneralSettings>();
   mapSettings_     = settings::MapSettings::Create();
   paletteSettings_ = settings::PaletteSettings::Create();
}

static bool LoadSettings(const boost::json::object& settingsJson)
{
   logger_->info("Loading settings");

   bool jsonDirty = false;

   generalSettings_ = std::make_shared<settings::GeneralSettings>();

   jsonDirty |= !generalSettings_->ReadJson(settingsJson);
   mapSettings_ =
      settings::MapSettings::Load(settingsJson.if_contains(kMapKey), jsonDirty);
   paletteSettings_ = settings::PaletteSettings::Load(
      settingsJson.if_contains(kPaletteKey), jsonDirty);

   return jsonDirty;
}

} // namespace SettingsManager
} // namespace manager
} // namespace qt
} // namespace scwx
