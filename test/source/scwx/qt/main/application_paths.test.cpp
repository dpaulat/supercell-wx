#include <scwx/qt/main/application_paths.hpp>
#include <scwx/qt/main/program_options.hpp>

#include <filesystem>

#include <gtest/gtest.h>
#include <QCoreApplication>

namespace scwx
{
namespace qt
{
namespace main
{

class ProgramOptionsTest : public ::testing::Test
{
protected:
   void SetUp() override { ProgramOptions::Reset(); }
   void TearDown() override { ProgramOptions::Reset(); }
};

class ApplicationPathsTest : public ::testing::Test
{
protected:
   void SetUp() override
   {
      ProgramOptions::Reset();
      ApplicationPaths::Reset();
   }

   void TearDown() override
   {
      ProgramOptions::Reset();
      ApplicationPaths::Reset();
   }
};

TEST_F(ProgramOptionsTest, ParseArguments_NoArgs)
{
   const char* argv[] = {"program"};
   ProgramOptions::ParseArguments({argv, 1});

   EXPECT_FALSE(ProgramOptions::GetOptions().showHelp_);
   EXPECT_FALSE(ProgramOptions::GetOptions().portableMode_);
   EXPECT_TRUE(ProgramOptions::GetOptions().settingsDirectory_.empty());
   EXPECT_TRUE(ProgramOptions::GetOptions().unrecognizedArgs_.empty());
}

TEST_F(ProgramOptionsTest, ParseArguments_PortableFlag)
{
   const char* argv[] = {"program", "--portable"};
   ProgramOptions::ParseArguments({argv, 2});

   EXPECT_TRUE(ProgramOptions::GetOptions().portableMode_);
   EXPECT_FALSE(ProgramOptions::GetOptions().showHelp_);
   EXPECT_TRUE(ProgramOptions::GetOptions().settingsDirectory_.empty());
}

TEST_F(ProgramOptionsTest, ParseArguments_ShortPortableFlag)
{
   const char* argv[] = {"program", "-p"};
   ProgramOptions::ParseArguments({argv, 2});

   EXPECT_TRUE(ProgramOptions::GetOptions().portableMode_);
}

TEST_F(ProgramOptionsTest, ParseArguments_SettingsDirectory)
{
   const char* argv[] = {"program", "--settings-directory", "/tmp/test_settings"};
   ProgramOptions::ParseArguments({argv, 3});

   EXPECT_EQ(ProgramOptions::GetOptions().settingsDirectory_, "/tmp/test_settings");
   EXPECT_FALSE(ProgramOptions::GetOptions().portableMode_);
}

TEST_F(ProgramOptionsTest, ParseArguments_UnrecognizedArgs)
{
   const char* argv[] = {"program", "--unknown-arg"};
   ProgramOptions::ParseArguments({argv, 2});

   EXPECT_FALSE(ProgramOptions::GetOptions().unrecognizedArgs_.empty());
   EXPECT_EQ(ProgramOptions::GetOptions().unrecognizedArgs_.front(),
             "--unknown-arg");
}

TEST_F(ProgramOptionsTest, ParseArguments_HelpFlag)
{
   const char* argv[] = {"program", "--help"};
   ProgramOptions::ParseArguments({argv, 2});

   EXPECT_TRUE(ProgramOptions::GetOptions().showHelp_);
}

TEST_F(ApplicationPathsTest, Initialize_SettingsDirectoryOverride)
{
   const std::filesystem::path testDir =
      std::filesystem::temp_directory_path() / "scwx_test_settings_override";

   std::filesystem::remove_all(testDir);

   const std::string testDirStr = testDir.string();
   const char* argv[] = {"program", "--settings-directory", testDirStr.c_str()};
   ProgramOptions::ParseArguments({argv, 3});
   ApplicationPaths::Initialize();

   EXPECT_EQ(
      ApplicationPaths::GetLocation(ApplicationPaths::StandardLocation::Settings),
      testDir);
   EXPECT_TRUE(std::filesystem::exists(testDir));

   std::filesystem::remove_all(testDir);
}

TEST_F(ApplicationPathsTest, Initialize_PortableAnchoring)
{
   const char* argv[] = {"program", "--portable"};
   ProgramOptions::ParseArguments({argv, 2});
   ApplicationPaths::Initialize();

   const std::string appDir =
      QCoreApplication::applicationDirPath().toStdString();
   ASSERT_FALSE(appDir.empty());

   const std::filesystem::path appDirPath(appDir);

   // Verify specific locations are under the application directory
   EXPECT_EQ(
      ApplicationPaths::GetLocation(ApplicationPaths::StandardLocation::Settings),
      appDirPath / "settings");
   EXPECT_EQ(
      ApplicationPaths::GetLocation(ApplicationPaths::StandardLocation::Cache),
      appDirPath / "cache");
   EXPECT_EQ(
      ApplicationPaths::GetLocation(ApplicationPaths::StandardLocation::Log),
      appDirPath / "logs");

   // Collect and clean up all created portable directories
   for (const auto location : ApplicationPaths::StandardLocationIterator())
   {
      const auto& path =
         ApplicationPaths::GetLocation(location);
      if (!path.empty())
      {
         std::filesystem::remove_all(path);
      }
   }
}

} // namespace main
} // namespace qt
} // namespace scwx
