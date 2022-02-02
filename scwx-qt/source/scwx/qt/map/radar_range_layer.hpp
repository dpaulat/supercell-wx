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

void Add(std::shared_ptr<QMapboxGL> map,
         float                      range,
         QMapbox::Coordinate        center,
         const QString&             before = QString());
void Update(std::shared_ptr<QMapboxGL> map,
            float                      range,
            QMapbox::Coordinate        center);

} // namespace RadarRangeLayer
} // namespace map
} // namespace qt
} // namespace scwx
