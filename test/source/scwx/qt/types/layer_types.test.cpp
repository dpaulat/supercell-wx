#include <scwx/qt/types/layer_types.hpp>

#include <boost/json.hpp>
#include <gtest/gtest.h>

namespace scwx::qt::types
{

TEST(LayerTypes, LayerSupportsOpacity)
{
   EXPECT_TRUE(LayerSupportsOpacity(LayerType::Radar));
   EXPECT_TRUE(LayerSupportsOpacity(LayerType::Alert));
   EXPECT_TRUE(LayerSupportsOpacity(LayerType::Placefile));
   EXPECT_TRUE(LayerSupportsOpacity(LayerType::Information));
   EXPECT_TRUE(LayerSupportsOpacity(LayerType::Data));
   EXPECT_FALSE(LayerSupportsOpacity(LayerType::Map));
   EXPECT_FALSE(LayerSupportsOpacity(LayerType::Unknown));
}

TEST(LayerTypes, OpacityPercentConversion)
{
   EXPECT_EQ(LayerOpacityToPercent(1.0f), 100);
   EXPECT_EQ(LayerOpacityToPercent(0.0f), 0);
   EXPECT_EQ(LayerOpacityToPercent(0.5f), 50);
   EXPECT_EQ(LayerOpacityToPercent(0.75f), 75);
   EXPECT_EQ(LayerOpacityToPercent(1.5f), 100);
   EXPECT_EQ(LayerOpacityToPercent(-0.2f), 0);

   EXPECT_FLOAT_EQ(LayerOpacityFromPercent(100), 1.0f);
   EXPECT_FLOAT_EQ(LayerOpacityFromPercent(0), 0.0f);
   EXPECT_FLOAT_EQ(LayerOpacityFromPercent(50), 0.5f);
   EXPECT_FLOAT_EQ(LayerOpacityFromPercent(150), 1.0f);
   EXPECT_FLOAT_EQ(LayerOpacityFromPercent(-10), 0.0f);
}

TEST(LayerTypes, OpacityJsonRoundTrip)
{
   LayerInfo original {.type_        = LayerType::Radar,
                       .description_ = std::monostate {},
                       .movable_     = true,
                       .opacity_     = 0.6f};

   const boost::json::value json     = boost::json::value_from(original);
   const LayerInfo          restored = boost::json::value_to<LayerInfo>(json);

   EXPECT_EQ(restored.type_, LayerType::Radar);
   EXPECT_FLOAT_EQ(restored.opacity_, 0.6f);
   ASSERT_TRUE(json.is_object());
   EXPECT_FLOAT_EQ(json.as_object().at("opacity").to_number<float>(), 0.6f);
}

TEST(LayerTypes, OpacityJsonDefaultsWhenMissing)
{
   const boost::json::value json = {
      {"type", "Radar"},
      {"description", ""},
      {"movable", true},
      {"displayed", {true, true, true, true, true, true, true, true, true}}};

   const LayerInfo restored = boost::json::value_to<LayerInfo>(json);
   EXPECT_EQ(restored.type_, LayerType::Radar);
   EXPECT_FLOAT_EQ(restored.opacity_, 1.0f);
}

TEST(LayerTypes, MapStyleOpacityStaysOpaque)
{
   LayerInfo mapLayer {.type_        = LayerType::Map,
                       .description_ = MapLayer::MapUnderlay,
                       .movable_     = false,
                       .opacity_     = 0.25f};

   const boost::json::value json     = boost::json::value_from(mapLayer);
   const LayerInfo          restored = boost::json::value_to<LayerInfo>(json);

   EXPECT_EQ(restored.type_, LayerType::Map);
   EXPECT_FLOAT_EQ(restored.opacity_, 1.0f);
   EXPECT_DOUBLE_EQ(json.as_object().at("opacity").to_number<double>(), 1.0);
}

TEST(LayerTypes, OpacityJsonClampsOutOfRange)
{
   const boost::json::value json = {{"type", "Radar"},
                                    {"description", ""},
                                    {"movable", true},
                                    {"displayed", {true}},
                                    {"opacity", 2.5}};

   const LayerInfo restored = boost::json::value_to<LayerInfo>(json);
   EXPECT_FLOAT_EQ(restored.opacity_, 1.0f);
}

} // namespace scwx::qt::types
