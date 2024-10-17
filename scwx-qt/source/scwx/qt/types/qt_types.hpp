#pragma once

#include <scwx/util/iterator.hpp>

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
   Unknown
};
typedef scwx::util::Iterator<UiStyle, UiStyle::Default, UiStyle::FusionDark>
   UiStyleIterator;

Qt::ColorScheme GetQtColorScheme(UiStyle uiStyle);
std::string     GetQtStyleName(UiStyle uiStyle);

UiStyle     GetUiStyle(const std::string& name);
std::string GetUiStyleName(UiStyle uiStyle);

} // namespace types
} // namespace qt
} // namespace scwx
