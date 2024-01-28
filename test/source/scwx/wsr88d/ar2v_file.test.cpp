#include <scwx/wsr88d/ar2v_file.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace wsr88d
{

class Ar2vValidFileTest :
    public testing::TestWithParam<std::pair<std::string, std::size_t>>
{
};

TEST_P(Ar2vValidFileTest, ValidFile)
{
   auto& param = GetParam();

   Ar2vFile file;
   bool     fileValid =
      file.LoadFile(std::string(SCWX_TEST_DATA_DIR) + param.first);

   EXPECT_EQ(fileValid, true);
   EXPECT_EQ(file.message_count(), param.second);
}

INSTANTIATE_TEST_SUITE_P(
   Ar2vFile,
   Ar2vValidFileTest,
   testing::Values(std::pair<std::string, std::size_t> //
                   {"/nexrad/level2/KCLE20021110_221234", 4031},
                   std::pair<std::string, std::size_t> //
                   {"/nexrad/level2/Level2_KLSX_20210527_1757.ar2v", 11167},
                   std::pair<std::string, std::size_t> //
                   {"/nexrad/level2/Level2_TSTL_20220213_2357.ar2v", 5763}));

} // namespace wsr88d
} // namespace scwx
