#pragma once

#include <scwx/common/types.hpp>

#include <memory>
#include <vector>

namespace scwx::qt::manager
{

class RadarCoordinateTable
{
public:
   RadarCoordinateTable(double latitude, double longitude, float gateSize);
   ~RadarCoordinateTable();

   RadarCoordinateTable(const RadarCoordinateTable&)            = delete;
   RadarCoordinateTable& operator=(const RadarCoordinateTable&) = delete;
   RadarCoordinateTable(RadarCoordinateTable&&)                 = delete;
   RadarCoordinateTable& operator=(RadarCoordinateTable&&)      = delete;

   [[nodiscard]] const std::vector<float>&
   coordinates(common::RadialSize radialSize, bool smoothingEnabled);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::manager
