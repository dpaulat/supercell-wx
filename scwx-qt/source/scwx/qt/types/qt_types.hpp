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

enum UiStyle
{
   Default,
   Fusion,
   Unknown
};
typedef scwx::util::Iterator<UiStyle, UiStyle::Default, UiStyle::Fusion>
   UiStyleIterator;

UiStyle     GetUiStyle(const std::string& name);
std::string GetUiStyleName(UiStyle alertAction);

} // namespace types
} // namespace qt
} // namespace scwx
