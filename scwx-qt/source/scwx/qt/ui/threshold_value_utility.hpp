#pragma once

#include <cmath>

namespace scwx::qt::ui
{

/** Steps per 1.0 physical unit on the color-table threshold slider. */
inline constexpr int kColorTableThresholdSliderStepsPerUnit = 10;

[[nodiscard]] inline float ColorTableThresholdSliderToPhysical(int slider_value,
                                                               float range_min)
{
   return range_min +
          static_cast<float>(slider_value) /
             static_cast<float>(kColorTableThresholdSliderStepsPerUnit);
}

/** Map physical threshold to slider integer (callers clamp to colormap). */
[[nodiscard]] inline int
ColorTableThresholdPhysicalToSlider(float physical_value, float range_min)
{
   return static_cast<int>(
      std::round((physical_value - range_min) *
                 static_cast<float>(kColorTableThresholdSliderStepsPerUnit)));
}

} // namespace scwx::qt::ui
