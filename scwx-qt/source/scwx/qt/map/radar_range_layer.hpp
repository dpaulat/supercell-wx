#pragma once

#include <qmaplibre.hpp>

namespace scwx::qt::map::RadarRangeLayer
{

void Add(const std::shared_ptr<QMapLibre::Map>& map,
         float                                  range,
         QMapLibre::Coordinate                  center,
         const QString&                         before  = QString(),
         float                                  opacity = 1.0f);
void Update(const std::shared_ptr<QMapLibre::Map>& map,
            float                                  range,
            QMapLibre::Coordinate                  center);
void SetOpacity(const std::shared_ptr<QMapLibre::Map>& map, float opacity);

} // namespace scwx::qt::map::RadarRangeLayer
