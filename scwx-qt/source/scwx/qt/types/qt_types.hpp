#pragma once

#include <scwx/util/iterator.hpp>

#include <optional>
#include <string>

#include <Qt>

namespace scwx
{
namespace qt
{
namespace types
{

enum ItemDataRole
{
   SortRole = Qt::UserRole,
   TimePointRole,
   RawDataRole
};

enum class UiStyle
{
   Default,
   Fusion,
   FusionLight,
   FusionDark,
   FusionAiry,
   FusionDarker,
   FusionDusk,
   FusionIaOra,
   FusionSand,
   FusionWaves,
   FusionCustom,
   Unknown
};
typedef scwx::util::Iterator<UiStyle, UiStyle::Default, UiStyle::FusionCustom>
   UiStyleIterator;

Qt::ColorScheme GetQtColorScheme(UiStyle uiStyle);
std::string     GetQtStyleName(UiStyle uiStyle);

std::optional<std::string> GetQtPaletteFile(UiStyle uiStyle);

UiStyle     GetUiStyle(const std::string& name);
std::string GetUiStyleName(UiStyle uiStyle);

} // namespace types
} // namespace qt
} // namespace scwx
