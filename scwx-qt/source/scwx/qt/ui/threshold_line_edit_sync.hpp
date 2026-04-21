#pragma once

#include <scwx/qt/ui/threshold_value_utility.hpp>

#include <QLineEdit>
#include <QLocale>
#include <QString>

#include <algorithm>
#include <cmath>

namespace scwx::qt::ui
{

/** `trimmed_text` parses and quantizes to same slider index as `slider_value`.
 */
[[nodiscard]] inline bool
ThresholdLineEditTextMatchesSlider(const QString& trimmed_text,
                                   int            slider_value,
                                   float          range_min,
                                   float          range_max)
{
   bool        ok     = false;
   const float parsed = QLocale::system().toFloat(trimmed_text, &ok);
   if (!ok || !std::isfinite(parsed))
   {
      return false;
   }
   const float clamped = std::clamp(parsed, range_min, range_max);
   const int   from_text =
      ColorTableThresholdPhysicalToSlider(clamped, range_min);
   return from_text == slider_value;
}

[[nodiscard]] inline bool ThresholdLineEditMatchesSlider(const QLineEdit& edit,
                                                         int   slider_value,
                                                         float range_min,
                                                         float range_max)
{
   return ThresholdLineEditTextMatchesSlider(
      edit.text().trimmed(), slider_value, range_min, range_max);
}

/**
 * When `force_line_edit` is false, avoid overwriting focused text that is an
 * incomplete number, but do overwrite when the committed parse disagrees with
 * `slider_value` (map/slider moved). When true, always apply the canonical
 * string.
 */
[[nodiscard]] inline bool
ShouldApplyThresholdLineEditText(const QLineEdit& edit,
                                 int              slider_value,
                                 float            range_min,
                                 float            range_max,
                                 bool             force_line_edit)
{
   if (force_line_edit)
   {
      return true;
   }
   if (!edit.hasFocus())
   {
      return true;
   }
   if (ThresholdLineEditMatchesSlider(edit, slider_value, range_min, range_max))
   {
      return false;
   }
   bool          ok      = false;
   const QString trimmed = edit.text().trimmed();
   const float   parsed  = QLocale::system().toFloat(trimmed, &ok);
   if (!ok || !std::isfinite(parsed))
   {
      return false;
   }
   const int from_text = ColorTableThresholdPhysicalToSlider(
      std::clamp(parsed, range_min, range_max), range_min);
   return from_text != slider_value;
}

} // namespace scwx::qt::ui
