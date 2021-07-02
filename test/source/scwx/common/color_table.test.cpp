#include <scwx/common/color_table.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace common
{

TEST(color_table, reflectivity)
{
   std::string filename(std::string(SCWX_TEST_DATA_DIR) +
                        "/colors/reflectivity.pal");

   std::shared_ptr<ColorTable> ct = ColorTable::Load(filename);

   EXPECT_EQ(ct->Color(5), boost::gil::rgba8_pixel_t(164, 164, 255, 255));
   EXPECT_EQ(ct->Color(10), boost::gil::rgba8_pixel_t(164, 164, 255, 255));
   EXPECT_EQ(ct->Color(20), boost::gil::rgba8_pixel_t(64, 128, 255, 255));
   EXPECT_EQ(ct->Color(30), boost::gil::rgba8_pixel_t(0, 255, 0, 255));
   EXPECT_EQ(ct->Color(32), boost::gil::rgba8_pixel_t(0, 230, 0, 255));
   EXPECT_EQ(ct->Color(35), boost::gil::rgba8_pixel_t(0, 192, 0, 255));
   EXPECT_EQ(ct->Color(40), boost::gil::rgba8_pixel_t(255, 255, 0, 255));
   EXPECT_EQ(ct->Color(50), boost::gil::rgba8_pixel_t(255, 0, 0, 255));
   EXPECT_EQ(ct->Color(55), boost::gil::rgba8_pixel_t(208, 0, 0, 255));
   EXPECT_EQ(ct->Color(60), boost::gil::rgba8_pixel_t(255, 0, 255, 255));
   EXPECT_EQ(ct->Color(65), boost::gil::rgba8_pixel_t(192, 0, 192, 255));
   EXPECT_EQ(ct->Color(70), boost::gil::rgba8_pixel_t(255, 255, 255, 255));
   EXPECT_EQ(ct->Color(80), boost::gil::rgba8_pixel_t(128, 128, 128, 255));
   EXPECT_EQ(ct->Color(85), boost::gil::rgba8_pixel_t(128, 128, 128, 255));
}

} // namespace common
} // namespace scwx
