#pragma once

#include <qmaplibre.hpp>

namespace scwx::qt::map::RadarRangeLayer
{

void Add(std::shared_ptr<QMapLibre::Map> map,
         float                           range,
         QMapLibre::Coordinate           center,
         const QString&                  before  = QString(),
         float                           opacity = 1.0f);
void Update(std::shared_ptr<QMapLibre::Map> map,
            float                           range,
            QMapLibre::Coordinate           center);
void SetOpacity(std::shared_ptr<QMapLibre::Map> map, float opacity);

} // namespace scwx::qt::map::RadarRangeLayer
