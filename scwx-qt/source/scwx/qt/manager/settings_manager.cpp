#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/util/json.hpp>

#include <filesystem>
#include <fstream>

#include <QDir>
#include <QStandardPaths>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace qt
{
namespace manager
{
namespace SettingsManager
{

static const std::string logPrefix_ = "[scwx::qt::manager::settings_manager] ";

static std::shared_ptr<settings::GeneralSettings> generalSettings_ = nullptr;
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
         BOOST_LOG_TRIVIAL(error)
            << logPrefix_ << "Unable to create application data directory: \""
            << appDataPath << "\"";
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

std::shared_ptr<settings::PaletteSettings> palette_settings()
{
   return paletteSettings_;
}

static boost::json::value ConvertSettingsToJson()
{
   boost::json::object settingsJson;

   settingsJson["general"] = generalSettings_->ToJson();
   settingsJson["palette"] = paletteSettings_->ToJson();

   return settingsJson;
}

static void GenerateDefaultSettings()
{
   BOOST_LOG_TRIVIAL(info) << logPrefix_ << "Generating default settings";

   generalSettings_ = settings::GeneralSettings::Create();
   paletteSettings_ = settings::PaletteSettings::Create();
}

static bool LoadSettings(const boost::json::object& settingsJson)
{
   BOOST_LOG_TRIVIAL(info) << logPrefix_ << "Loading settings";

   bool jsonDirty = false;

   generalSettings_ = settings::GeneralSettings::Load(
      settingsJson.if_contains("general"), jsonDirty);
   paletteSettings_ = settings::PaletteSettings::Load(
      settingsJson.if_contains("palette"), jsonDirty);

   return jsonDirty;
}

} // namespace SettingsManager
} // namespace manager
} // namespace qt
} // namespace scwx
