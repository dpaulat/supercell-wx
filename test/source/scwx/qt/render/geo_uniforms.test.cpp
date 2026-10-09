#include <scwx/qt/render/rhi_geo_uniforms.hpp>
#include <scwx/qt/render/rhi_radar_overlay.hpp>

#include <cstddef>

#include <gtest/gtest.h>

namespace scwx::qt::render
{

TEST(GeoUniforms, Std140Layout)
{
   EXPECT_EQ(sizeof(GeoUniforms), 160u);
   EXPECT_EQ(offsetof(GeoUniforms, uOpacity), 144u);
   EXPECT_FLOAT_EQ(GeoUniforms {}.uOpacity, 1.0f);
}

TEST(RadarUniforms, Std140Layout)
{
   EXPECT_EQ(sizeof(RadarUniforms), 160u);
   EXPECT_EQ(offsetof(RadarUniforms, uOpacity), 148u);
   EXPECT_FLOAT_EQ(RadarUniforms {}.uOpacity, 1.0f);
}

} // namespace scwx::qt::render
