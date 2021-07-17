#pragma once

#include <QMapboxGL>

namespace scwx
{
namespace qt
{

namespace RadarRangeLayer
{
void Add(std::shared_ptr<QMapboxGL> map, const QString& before = QString());
};

} // namespace qt
} // namespace scwx
