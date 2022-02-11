#include <scwx/wsr88d/nexrad_file_factory.hpp>
#include <scwx/wsr88d/ar2v_file.hpp>
#include <scwx/wsr88d/level3_file.hpp>

#include <gtest/gtest.h>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{

static const std::string logPrefix_ =
   "[scwx::wsr88d::nexrad_file_factory.test] ";

TEST(NexradFileFactory, Level2V06)
{
   std::string filename = std::string(SCWX_TEST_DATA_DIR) +
                          "/nexrad/level2/Level2_KLSX_20210527_1757.ar2v";

   std::shared_ptr<NexradFile> file = NexradFileFactory::Create(filename);
   std::shared_ptr<Ar2vFile>   level2File =
      std::dynamic_pointer_cast<Ar2vFile>(file);

   EXPECT_NE(file, nullptr);
   EXPECT_NE(level2File, nullptr);
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
}

} // namespace wsr88d
} // namespace scwx
