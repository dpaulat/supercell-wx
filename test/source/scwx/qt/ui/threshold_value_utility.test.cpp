#include <scwx/qt/ui/threshold_line_edit_sync.hpp>
#include <scwx/qt/ui/threshold_value_utility.hpp>

#include <gtest/gtest.h>

#include <algorithm>

namespace scwx::qt::ui
{
namespace
{

TEST(ThresholdValueUtility, SliderToPhysicalAtMin)
{
   EXPECT_FLOAT_EQ(ColorTableThresholdSliderToPhysical(0, -32.0f), -32.0f);
}

TEST(ThresholdValueUtility, SliderToPhysicalOneStep)
{
   EXPECT_FLOAT_EQ(ColorTableThresholdSliderToPhysical(1, -32.0f), -31.9f);
}

TEST(ThresholdValueUtility, PhysicalToSliderRoundTrip)
{
   constexpr float kMin {-10.0f};
   constexpr float kMax {50.0f};
   for (int slider = 0; slider <= 600; slider += 50)
   {
      const float physical = ColorTableThresholdSliderToPhysical(slider, kMin);
      const float clamped  = std::clamp(physical, kMin, kMax);
      const int   back     = ColorTableThresholdPhysicalToSlider(clamped, kMin);
      EXPECT_EQ(back, slider);
   }
}

TEST(ThresholdValueUtility, PhysicalToSliderMatchesTenStepsPerUnit)
{
   constexpr float kMin {0.0f};
   EXPECT_EQ(ColorTableThresholdPhysicalToSlider(0.0f, kMin), 0);
   EXPECT_EQ(ColorTableThresholdPhysicalToSlider(0.04f, kMin), 0);
   EXPECT_EQ(ColorTableThresholdPhysicalToSlider(0.05f, kMin), 1);
   EXPECT_EQ(ColorTableThresholdPhysicalToSlider(1.0f, kMin), 10);
}

TEST(ThresholdLineEditSync, TextMatchesSliderQuantized)
{
   constexpr float kMin {-32.0f};
   constexpr float kMax {94.5f};
   EXPECT_TRUE(ThresholdLineEditTextMatchesSlider(
      QStringLiteral("14.2"), 462, kMin, kMax));
   EXPECT_FALSE(ThresholdLineEditTextMatchesSlider(
      QStringLiteral("14.3"), 462, kMin, kMax));
}

TEST(ThresholdLineEditSync, InvalidTextDoesNotMatch)
{
   EXPECT_FALSE(ThresholdLineEditTextMatchesSlider(
      QStringLiteral(".."), 0, -32.0f, 94.5f));
}

} // namespace
} // namespace scwx::qt::ui
