#pragma once

#include <qmaplibre.hpp>

namespace scwx
{
namespace qt
{
namespace map
{
namespace RadarRangeLayer
{

void Add(std::shared_ptr<QMapLibre::Map> map,
         float                           range,
         QMapLibre::Coordinate           center,
         const QString&                  before = QString());
void Update(std::shared_ptr<QMapLibre::Map> map,
            float                           range,
            QMapLibre::Coordinate           center);

} // namespace RadarRangeLayer
} // namespace map
} // namespace qt
} // namespace scwx
