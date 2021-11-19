#include <scwx/qt/manager/settings_manager.hpp>

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

class DefaultSettingsTest : public testing::TestWithParam<std::string>
{
};

void VerifyDefaults()
{
   std::shared_ptr<settings::GeneralSettings> defaultGeneralSettings =
      settings::GeneralSettings::Create();
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

TEST(SettingsManager, CreateJson)
{
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

TEST(SettingsManager, SettingsKeax)
{
   std::string filename(std::string(SCWX_TEST_DATA_DIR) +
                        "/json/settings/settings-keax.json");

   SettingsManager::ReadSettings(filename);

   EXPECT_EQ(SettingsManager::general_settings()->default_radar_site(), "KEAX");
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
                                         "settings-empty-groups.json",
                                         "settings-empty-object.json"));

TEST(SettingsManager, SettingsBadMinimum)
{
   std::string minimumFile(std::string(SCWX_TEST_DATA_DIR) +
                           "/json/settings/settings-minimum.json");
   std::string sourceFile(std::string(SCWX_TEST_DATA_DIR) +
                          "/json/settings/settings-bad-minimum.json");
   std::string filename {TEMP_SETTINGS_FILE};

   std::filesystem::copy_file(sourceFile, filename);

   SettingsManager::ReadSettings(filename);

   CompareFiles(filename, minimumFile);

   std::filesystem::remove(filename);
}

TEST(SettingsManager, SettingsBadMaximum)
{
   std::string maximumFile(std::string(SCWX_TEST_DATA_DIR) +
                           "/json/settings/settings-maximum.json");
   std::string sourceFile(std::string(SCWX_TEST_DATA_DIR) +
                          "/json/settings/settings-bad-maximum.json");
   std::string filename {TEMP_SETTINGS_FILE};

   std::filesystem::copy_file(sourceFile, filename);

   SettingsManager::ReadSettings(filename);

   CompareFiles(filename, maximumFile);

   std::filesystem::remove(filename);
}

} // namespace manager
} // namespace qt
} // namespace scwx
