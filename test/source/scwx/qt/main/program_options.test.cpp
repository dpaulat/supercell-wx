#include <scwx/qt/main/program_options.hpp>

#include <gtest/gtest.h>
#include <QCoreApplication>

namespace scwx::qt::main
{

class ProgramOptionsTest : public ::testing::Test
{
protected:
   void SetUp() override { ProgramOptions::Reset(); }
   void TearDown() override { ProgramOptions::Reset(); }
};

TEST_F(ProgramOptionsTest, ParseArguments_NoArgs)
{
   const char* argv[] = {"program"};
   ProgramOptions::ParseArguments({argv, 1});

   EXPECT_FALSE(ProgramOptions::GetOptions().showHelp_);
   EXPECT_FALSE(ProgramOptions::GetOptions().enableConsole_);
   EXPECT_FALSE(ProgramOptions::GetOptions().portableMode_);
   EXPECT_TRUE(ProgramOptions::GetOptions().settingsDirectory_.empty());
   EXPECT_TRUE(ProgramOptions::GetOptions().unrecognizedArgs_.empty());
}

TEST_F(ProgramOptionsTest, ParseArguments_PortableFlag)
{
   const char* argv[] = {"program", "--portable"};
   ProgramOptions::ParseArguments({argv, 2});

   EXPECT_TRUE(ProgramOptions::GetOptions().portableMode_);
   EXPECT_FALSE(ProgramOptions::GetOptions().enableConsole_);
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
   const char* argv[] = {
      "program", "--settings-directory", "/tmp/test_settings"};
   ProgramOptions::ParseArguments({argv, 3});

   EXPECT_EQ(ProgramOptions::GetOptions().settingsDirectory_,
             "/tmp/test_settings");
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

} // namespace scwx::qt::main
