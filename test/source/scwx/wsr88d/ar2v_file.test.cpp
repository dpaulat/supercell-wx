#include <scwx/wsr88d/ar2v_file.hpp>

#include <fstream>

#include <gtest/gtest.h>

namespace scwx
{
namespace wsr88d
{

TEST(ar2v_file, klsx)
{
   Ar2vFile file;
   bool     fileValid =
      file.LoadFile(std::string(SCWX_TEST_DATA_DIR) +
                    "/nexrad/level2/Level2_KLSX_20210527_1757.ar2v");

   EXPECT_EQ(fileValid, true);
}

TEST(ar2v_file, tstl)
{
   Ar2vFile file;
   bool     fileValid =
      file.LoadFile(std::string(SCWX_TEST_DATA_DIR) +
                    "/nexrad/level2/Level2_TSTL_20220213_2357.ar2v");

   EXPECT_EQ(fileValid, true);
}

} // namespace wsr88d
} // namespace scwx
