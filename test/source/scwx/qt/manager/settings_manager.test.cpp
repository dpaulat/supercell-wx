#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/settings/map_settings.hpp>
#include <scwx/qt/settings/palette_settings.hpp>
#include <scwx/qt/settings/text_settings.hpp>
#include <scwx/qt/settings/ui_settings.hpp>
#include <scwx/util/json.hpp>

#include <filesystem>

#include <gtest/gtest.h>

namespace scwx::qt::manager
{

static const std::string DEFAULT_SETTINGS_FILE =
   std::string(SCWX_TEST_DATA_DIR) + "/json/settings/settings-default.json";
static const std::string TEMP_SETTINGS_FILE =
   std::string(SCWX_TEST_DATA_DIR) + "/json/settings/settings-temp.json";

class SettingsManagerTest : public testing::Test
{
   virtual void SetUp() { scwx::qt::config::RadarSite::Initialize(); }
};

class DefaultSettingsTest : public testing::TestWithParam<std::string>
{
   virtual void SetUp() { scwx::qt::config::RadarSite::Initialize(); }
};

class BadSettingsTest :
    public testing::TestWithParam<std::pair<std::string, std::string>>
{
   virtual void SetUp() { scwx::qt::config::RadarSite::Initialize(); }
};

static void VerifyDefaults()
{
   settings::GeneralSettings defaultGeneralSettings {};
   settings::MapSettings     defaultMapSettings {};
   settings::PaletteSettings defaultPaletteSettings {};
   settings::TextSettings    defaultTextSettings {};
   settings::UiSettings      defaultUiSettings {};

   EXPECT_EQ(defaultGeneralSettings, settings::GeneralSettings::Instance());
   EXPECT_EQ(defaultMapSettings, settings::MapSettings::Instance());
   EXPECT_EQ(defaultPaletteSettings, settings::PaletteSettings::Instance());
   EXPECT_EQ(defaultTextSettings, settings::TextSettings::Instance());
   EXPECT_EQ(defaultUiSettings, settings::UiSettings::Instance());
}

static void RemoveUserPaths(boost::json::value& root)
{
   // Check if root is an object
   if (!root.is_object())
      return;

   boost::json::object& obj = root.as_object();

   // Look for the "general" object
   auto generalIt = obj.find("general");
   if (generalIt != obj.end() && generalIt->value().is_object())
   {
      boost::json::object& generalObj = generalIt->value().as_object();

      // Remove the "screen_capture_folder" key if it exists
      auto folderIt = generalObj.find("screen_capture_folder");
      if (folderIt != generalObj.end())
      {
         folderIt->value() = "";
      }
   }
}

static void RemoveTransientUiState(boost::json::value& root)
{
   if (!root.is_object())
   {
      return;
   }

   boost::json::object& obj  = root.as_object();
   auto                 uiIt = obj.find("ui");
   if (uiIt == obj.end() || !uiIt->value().is_object())
   {
      return;
   }

   boost::json::object& uiObj = uiIt->value().as_object();
   uiObj.erase("map_annotation_state");
}

static void NormalizeProductDefaults(boost::json::value& root)
{
   if (!root.is_object())
   {
      return;
   }

   boost::json::object& obj = root.as_object();
   auto                 productIt = obj.find("product");
   if (productIt == obj.end() || !productIt->value().is_object())
   {
      return;
   }

   boost::json::object& productObj = productIt->value().as_object();
   if (!productObj.contains("hail_index_enabled"))
   {
      productObj["hail_index_enabled"] = true;
   }
   if (!productObj.contains("mesocyclone_enabled"))
   {
      productObj["mesocyclone_enabled"] = true;
   }
   if (!productObj.contains("tvs_enabled"))
   {
      productObj["tvs_enabled"] = true;
   }
}

static void CompareFiles(const std::string& file1, const std::string& file2)
{
   auto jf1 = scwx::util::json::ReadJsonFile(file1);
   auto jf2 = scwx::util::json::ReadJsonFile(file2);

   RemoveUserPaths(jf1);
   RemoveUserPaths(jf2);
   RemoveTransientUiState(jf1);
   RemoveTransientUiState(jf2);
   NormalizeProductDefaults(jf1);
   NormalizeProductDefaults(jf2);

   EXPECT_EQ(jf1, jf2);
}

TEST_F(SettingsManagerTest, CreateJson)
{
   scwx::qt::config::RadarSite::Initialize();

   std::string filename {TEMP_SETTINGS_FILE};

   // Verify file doesn't exist prior to test start
   EXPECT_EQ(std::filesystem::exists(filename), false);

   SettingsManager::Instance().ReadSettings(filename);

   EXPECT_EQ(std::filesystem::exists(filename), true);

   VerifyDefaults();
   CompareFiles(filename, DEFAULT_SETTINGS_FILE);

   std::filesystem::remove(filename);
   EXPECT_EQ(std::filesystem::exists(filename), false);
}

TEST_F(SettingsManagerTest, SettingsKeax)
{
   std::string filename(std::string(SCWX_TEST_DATA_DIR) +
                        "/json/settings/settings-keax.json");

   SettingsManager::Instance().ReadSettings(filename);

   EXPECT_EQ(
      settings::GeneralSettings::Instance().default_radar_site().GetValue(),
      "KEAX");
   for (size_t i = 0; i < settings::MapSettings::Instance().count(); ++i)
   {
      EXPECT_EQ(settings::MapSettings::Instance().radar_site(i).GetValue(),
                "KEAX");
   }
}

TEST_P(DefaultSettingsTest, DefaultSettings)
{
   std::string sourceFile(std::string(SCWX_TEST_DATA_DIR) + "/json/settings/" +
                          GetParam());
   std::string filename {TEMP_SETTINGS_FILE};

   std::filesystem::copy_file(sourceFile, filename);

   SettingsManager::Instance().ReadSettings(filename);

   VerifyDefaults();
   CompareFiles(filename, DEFAULT_SETTINGS_FILE);

   std::filesystem::remove(filename);
}

INSTANTIATE_TEST_SUITE_P(SettingsManager,
                         DefaultSettingsTest,
                         testing::Values("settings-bad-types.json",
                                         "settings-bad-types2.json",
                                         "settings-empty-arrays.json",
                                         "settings-empty-groups.json",
                                         "settings-empty-object.json"));

TEST_P(BadSettingsTest, BadSettings)
{
   auto& [goodFilename, badFilename] = GetParam();

   const std::string goodFile(std::string(SCWX_TEST_DATA_DIR) +
                              "/json/settings/" + goodFilename);
   const std::string sourceFile(std::string(SCWX_TEST_DATA_DIR) +
                                "/json/settings/" + badFilename);
   const std::string filename {TEMP_SETTINGS_FILE};

   std::filesystem::copy_file(sourceFile, filename);

   SettingsManager::Instance().ReadSettings(filename);

   CompareFiles(filename, goodFile);

   std::filesystem::remove(filename);
}

INSTANTIATE_TEST_SUITE_P(
   SettingsManager,
   BadSettingsTest,
   testing::Values(
      std::make_pair("settings-minimum.json", "settings-bad-minimum.json"),
      std::make_pair("settings-maximum.json", "settings-bad-maximum.json"),
      std::make_pair("settings-maps.json", "settings-bad-maps.json")));

} // namespace scwx::qt::manager
