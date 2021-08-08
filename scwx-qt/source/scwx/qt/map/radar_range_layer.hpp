#pragma once

#include <QMapboxGL>

namespace scwx
{
namespace qt
{
namespace map
{

namespace RadarRangeLayer
{
void Add(std::shared_ptr<QMapboxGL> map, const QString& before = QString());
};

} // namespace map
} // namespace qt
} // namespace scwx
