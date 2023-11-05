#pragma once

#include <string>

#include <QPoint>

namespace scwx
{
namespace qt
{
namespace util
{
namespace tooltip
{

void Show(const std::string& text, const QPointF& mouseGlobalPos);
void Hide();

} // namespace tooltip
} // namespace util
} // namespace qt
} // namespace scwx
