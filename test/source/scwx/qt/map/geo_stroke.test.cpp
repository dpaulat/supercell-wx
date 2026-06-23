#include <scwx/qt/map/geo_stroke.hpp>

#include <gtest/gtest.h>

namespace scwx::qt::map
{

TEST(GeoStroke, zero_widths)
{
   const GeoStrokeHalfWidths widths =
      ComputeGeoStrokeHalfWidths(0.0f, 0.0f, 0.0f);
   EXPECT_FLOAT_EQ(widths.lineHalf_, 0.0f);
   EXPECT_FLOAT_EQ(widths.highlightHalf_, 0.0f);
   EXPECT_FLOAT_EQ(widths.borderHalf_, 0.0f);
}

TEST(GeoStroke, nested_bands)
{
   const GeoStrokeHalfWidths widths =
      ComputeGeoStrokeHalfWidths(4.0f, 2.0f, 1.0f);
   EXPECT_FLOAT_EQ(widths.lineHalf_, 2.0f);
   EXPECT_FLOAT_EQ(widths.highlightHalf_, 4.0f);
   EXPECT_FLOAT_EQ(widths.borderHalf_, 5.0f);
}

TEST(GeoStroke, negative_inputs_clamped)
{
   const GeoStrokeHalfWidths widths =
      ComputeGeoStrokeHalfWidths(4.0f, -1.0f, -2.0f);
   EXPECT_FLOAT_EQ(widths.lineHalf_, 2.0f);
   EXPECT_FLOAT_EQ(widths.highlightHalf_, 2.0f);
   EXPECT_FLOAT_EQ(widths.borderHalf_, 2.0f);
}

} // namespace scwx::qt::map
