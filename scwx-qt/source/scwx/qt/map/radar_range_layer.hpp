#pragma once

#include <QMapLibreGL/QMapLibreGL>

namespace scwx
{
namespace qt
{
namespace map
{
namespace RadarRangeLayer
{

void Add(std::shared_ptr<QMapLibreGL::Map> map,
         float                             range,
         QMapLibreGL::Coordinate           center,
         const QString&                    before = QString());
void Update(std::shared_ptr<QMapLibreGL::Map> map,
            float                             range,
            QMapLibreGL::Coordinate           center);

} // namespace RadarRangeLayer
} // namespace map
} // namespace qt
} // namespace scwx
