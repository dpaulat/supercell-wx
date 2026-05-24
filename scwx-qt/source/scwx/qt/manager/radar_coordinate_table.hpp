#pragma once

#include <scwx/common/constants.hpp>
#include <scwx/common/products.hpp>
#include <scwx/common/types.hpp>

#include <cstdint>
#include <mutex>
#include <vector>

#include <units/angle.h>

namespace scwx::qt::manager
{

class RadarCoordinateTable
{
public:
   RadarCoordinateTable(double latitude, double longitude, float gateSize);

   [[nodiscard]] const std::vector<float>&
   coordinates(common::RadialSize radialSize, bool smoothingEnabled);

private:
   void CalculateCoordinates(uint32_t                     radialCount,
                             units::angle::degrees<float> radialAngle,
                             units::angle::degrees<float> angleOffset,
                             float                        gateRangeOffset,
                             std::vector<float>&          outputCoordinates);

   void EnsureCoordinatesInitialized(common::RadialSize radialSize,
                                     bool               smoothingEnabled);

   double latitude_;
   double longitude_;
   float  gateSize_;

   std::vector<float> coordinates0_5Degree_ {};
   std::vector<float> coordinates0_5DegreeSmooth_ {};
   std::vector<float> coordinates1Degree_ {};
   std::vector<float> coordinates1DegreeSmooth_ {};

   std::mutex coordinatesMutex_ {};
};

} // namespace scwx::qt::manager
