#include <scwx/qt/map/geo_stroke.hpp>

#include <gtest/gtest.h>

namespace scwx::qt::map
{

TEST(GeoStrokeBand, center_is_line)
{
   const GeoStrokeHalfWidths widths =
      ComputeGeoStrokeHalfWidths(4.0f, 2.0f, 1.0f);
   EXPECT_EQ(ClassifyGeoStrokeBand(0.0f, widths), GeoStrokeBand::Line);
   EXPECT_EQ(ClassifyGeoStrokeBand(1.5f, widths), GeoStrokeBand::Line);
}

TEST(GeoStrokeBand, highlight_and_border_bands)
{
   const GeoStrokeHalfWidths widths =
      ComputeGeoStrokeHalfWidths(4.0f, 2.0f, 1.0f);
   EXPECT_FLOAT_EQ(widths.lineHalf_, 2.0f);
   EXPECT_FLOAT_EQ(widths.highlightHalf_, 4.0f);
   EXPECT_FLOAT_EQ(widths.borderHalf_, 5.0f);

   EXPECT_EQ(ClassifyGeoStrokeBand(3.5f, widths), GeoStrokeBand::Highlight);
   EXPECT_EQ(ClassifyGeoStrokeBand(4.5f, widths), GeoStrokeBand::Border);
   EXPECT_EQ(ClassifyGeoStrokeBand(5.5f, widths), GeoStrokeBand::Outside);
}

TEST(GeoStrokeBand, negative_offset_symmetric)
{
   const GeoStrokeHalfWidths widths =
      ComputeGeoStrokeHalfWidths(4.0f, 2.0f, 1.0f);
   EXPECT_EQ(ClassifyGeoStrokeBand(-0.5f, widths), GeoStrokeBand::Line);
   EXPECT_EQ(ClassifyGeoStrokeBand(-5.5f, widths), GeoStrokeBand::Outside);
}

TEST(GeoStrokeBand, stroke_disabled_when_no_border)
{
   const GeoStrokeHalfWidths widths {.lineHalf_ = 2.0f,
                                     .highlightHalf_ = 2.0f,
                                     .borderHalf_ = 0.0f};
   EXPECT_EQ(ClassifyGeoStrokeBand(99.0f, widths), GeoStrokeBand::Line);
}

TEST(GeoStrokeBand, outer_width_matches_border_diameter)
{
   const GeoStrokeHalfWidths widths =
      ComputeGeoStrokeHalfWidths(4.0f, 2.0f, 1.0f);
   EXPECT_FLOAT_EQ(GeoStrokeOuterWidth(widths), widths.borderHalf_ * 2.0f);
   EXPECT_FLOAT_EQ(GeoStrokeOuterWidth(widths), 10.0f);
}

} // namespace scwx::qt::map
