#include <scwx/wsr88d/nexrad_file_factory.hpp>
#include <scwx/wsr88d/ar2v_file.hpp>
#include <scwx/wsr88d/level3_file.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <gtest/gtest.h>

namespace scwx
{
namespace wsr88d
{

static const std::string logPrefix_ = "scwx::wsr88d::nexrad_file_factory.test";

static std::string ReadAllBytes(const std::string& filename)
{
   std::ifstream is(filename, std::ios_base::in | std::ios_base::binary);
   return {std::istreambuf_iterator<char>(is),
           std::istreambuf_iterator<char>()};
}

TEST(NexradFileFactory, Level2V06)
{
   std::string filename = std::string(SCWX_TEST_DATA_DIR) +
                          "/nexrad/level2/Level2_KLSX_20210527_1757.ar2v";

   std::shared_ptr<NexradFile> file = NexradFileFactory::Create(filename);
   std::shared_ptr<Ar2vFile>   level2File =
      std::dynamic_pointer_cast<Ar2vFile>(file);

   EXPECT_NE(file, nullptr);
   EXPECT_NE(level2File, nullptr);
   EXPECT_EQ(file->filename(), "Level2_KLSX_20210527_1757.ar2v");
   EXPECT_TRUE(file->has_file_data());
   EXPECT_FALSE(file->is_gzip_compressed());
}

TEST(NexradFileFactory, Level2V06Gzip)
{
   std::string filename = std::string(SCWX_TEST_DATA_DIR) +
                          "/nexrad/level2/KLSX20130206_175044_V06.gz";

   std::shared_ptr<NexradFile> file = NexradFileFactory::Create(filename);
   std::shared_ptr<Ar2vFile>   level2File =
      std::dynamic_pointer_cast<Ar2vFile>(file);

   EXPECT_NE(file, nullptr);
   EXPECT_NE(level2File, nullptr);
   EXPECT_EQ(file->filename(), "KLSX20130206_175044_V06.gz");
   EXPECT_TRUE(file->has_file_data());
   EXPECT_TRUE(file->is_gzip_compressed());
}

TEST(NexradFileFactory, Level3)
{
   std::string filename = std::string(SCWX_TEST_DATA_DIR) +
                          "/nexrad/level3/KLSX_SDUS23_N2QLSX_202112110250";

   std::shared_ptr<NexradFile> file = NexradFileFactory::Create(filename);
   std::shared_ptr<Level3File> level3File =
      std::dynamic_pointer_cast<Level3File>(file);

   EXPECT_NE(file, nullptr);
   EXPECT_NE(level3File, nullptr);
   EXPECT_EQ(file->filename(), "KLSX_SDUS23_N2QLSX_202112110250");
   EXPECT_TRUE(file->has_file_data());
}

class NexradFileSaveTest : public testing::TestWithParam<std::string>
{
};

TEST_P(NexradFileSaveTest, SaveRoundTrip)
{
   const std::string filename = std::string(SCWX_TEST_DATA_DIR) + GetParam();

   const std::shared_ptr<NexradFile> file = NexradFileFactory::Create(filename);
   ASSERT_NE(file, nullptr);
   ASSERT_TRUE(file->has_file_data());

   const auto tempPath = std::filesystem::temp_directory_path() /
                         ("scwx_nexrad_export_test_" + file->filename());
   std::filesystem::remove(tempPath);

   ASSERT_TRUE(file->SaveFile(tempPath.string()));
   EXPECT_EQ(ReadAllBytes(filename), ReadAllBytes(tempPath.string()));

   std::filesystem::remove(tempPath);
}

INSTANTIATE_TEST_SUITE_P(
   NexradFileFactory,
   NexradFileSaveTest,
   testing::Values("/nexrad/level2/Level2_KLSX_20210527_1757.ar2v",
                   "/nexrad/level2/KLSX20130206_175044_V06.gz",
                   "/nexrad/level3/KLSX_SDUS23_N2QLSX_202112110250"));

TEST(NexradFile, SaveFileWithoutData)
{
   const Ar2vFile file;
   const auto     tempPath =
      std::filesystem::temp_directory_path() / "scwx_nexrad_export_missing";

   EXPECT_FALSE(file.has_file_data());
   EXPECT_FALSE(file.SaveFile(tempPath.string()));
}

} // namespace wsr88d
} // namespace scwx
