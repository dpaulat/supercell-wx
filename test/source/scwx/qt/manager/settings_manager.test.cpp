#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/config/radar_site.hpp>

#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

namespace scwx
{
namespace qt
{
namespace manager
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

void VerifyDefaults()
{
   std::shared_ptr<settings::GeneralSettings> defaultGeneralSettings =
      std::make_shared<settings::GeneralSettings>();
   std::shared_ptr<settings::PaletteSettings> defaultPaletteSettings =
      settings::PaletteSettings::Create();

   EXPECT_EQ(*defaultGeneralSettings, *SettingsManager::general_settings());
   EXPECT_EQ(*defaultPaletteSettings, *SettingsManager::palette_settings());
}

void CompareFiles(const std::string& file1, const std::string& file2)
{
   std::ifstream     ifs1 {file1};
   std::stringstream buffer1;
   buffer1 << ifs1.rdbuf();

   std::ifstream     ifs2 {file2};
   std::stringstream buffer2;
   buffer2 << ifs2.rdbuf();

   EXPECT_EQ(buffer1.str(), buffer2.str());
}

TEST_F(SettingsManagerTest, CreateJson)
{
   scwx::qt::config::RadarSite::Initialize();

   std::string filename {TEMP_SETTINGS_FILE};

   // Verify file doesn't exist prior to test start
   EXPECT_EQ(std::filesystem::exists(filename), false);

   SettingsManager::ReadSettings(filename);

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

   SettingsManager::ReadSettings(filename);

   EXPECT_EQ(SettingsManager::general_settings()->default_radar_site(), "KEAX");
   for (size_t i = 0; i < SettingsManager::map_settings()->count(); ++i)
   {
      EXPECT_EQ(SettingsManager::map_settings()->radar_site(i), "KEAX");
   }
}

TEST_P(DefaultSettingsTest, DefaultSettings)
{
   std::string sourceFile(std::string(SCWX_TEST_DATA_DIR) + "/json/settings/" +
                          GetParam());
   std::string filename {TEMP_SETTINGS_FILE};

   std::filesystem::copy_file(sourceFile, filename);

   SettingsManager::ReadSettings(filename);

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

   SettingsManager::ReadSettings(filename);

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

} // namespace manager
} // namespace qt
} // namespace scwx
