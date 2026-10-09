#include <scwx/qt/map/geo_stroke.hpp>

#include <algorithm>

namespace scwx::qt::map
{

GeoStrokeHalfWidths ComputeGeoStrokeHalfWidths(const float lineWidth,
                                               const float highlightWidth,
                                               const float borderWidth) noexcept
{
   const float lineHalf      = std::max(0.0f, lineWidth * 0.5f);
   const float highlightHalf = lineHalf + std::max(0.0f, highlightWidth);
   const float borderHalf    = highlightHalf + std::max(0.0f, borderWidth);

   return GeoStrokeHalfWidths {.lineHalf_      = lineHalf,
                               .highlightHalf_ = highlightHalf,
                               .borderHalf_    = borderHalf};
}

GeoStrokeBand ClassifyGeoStrokeBand(const float                offsetY,
                                    const GeoStrokeHalfWidths& widths) noexcept
{
   if (widths.borderHalf_ <= 0.0f)
   {
      return GeoStrokeBand::Line;
   }

   const float d = std::abs(offsetY);
   if (d > widths.borderHalf_)
   {
      return GeoStrokeBand::Outside;
   }
   if (d > widths.highlightHalf_)
   {
      return GeoStrokeBand::Border;
   }
   if (d > widths.lineHalf_)
   {
      return GeoStrokeBand::Highlight;
   }
   return GeoStrokeBand::Line;
}

float GeoStrokeOuterWidth(const GeoStrokeHalfWidths& widths) noexcept
{
   return widths.borderHalf_ * 2.0f;
}

} // namespace scwx::qt::map
