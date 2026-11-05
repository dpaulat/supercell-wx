#pragma once

namespace scwx::qt::map
{

struct GeoStrokeHalfWidths
{
   float lineHalf_ {};
   float highlightHalf_ {};
   float borderHalf_ {};
};

enum class GeoStrokeBand
{
   Line,
   Highlight,
   Border,
   Outside
};

/**
 * @brief Pixel half-widths from centerline for nested line / highlight / border
 * bands.
 */
[[nodiscard]] GeoStrokeHalfWidths ComputeGeoStrokeHalfWidths(
   float lineWidth, float highlightWidth, float borderWidth) noexcept;

/** Mirrors geo_color.frag band selection (offset = |aXYOffset.x|, the
 * perpendicular pixel offset from the centerline). */
[[nodiscard]] GeoStrokeBand
ClassifyGeoStrokeBand(float                      offsetY,
                      const GeoStrokeHalfWidths& widths) noexcept;

/** Total stroke width in pixels (2 * borderHalf). */
[[nodiscard]] float
GeoStrokeOuterWidth(const GeoStrokeHalfWidths& widths) noexcept;

} // namespace scwx::qt::map
