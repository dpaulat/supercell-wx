#pragma once

#include <scwx/wsr88d/wsr88d_types.hpp>

namespace scwx
{
namespace wsr88d
{

class DualPolInterpreter
{
public:
   /**
    * @brief Classifies the hydrometeor type based on dual-pol moments.
    *
    * @param [in] z Reflectivity (dBZ)
    * @param [in] zdr Differential Reflectivity (dB)
    * @param [in] rhohv Correlation Coefficient (unitless)
    *
    * @return Hydrometeor classification
    */
   static DataLevelCode ClassifyHydrometeor(float z, float zdr, float rhohv);

   /**
    * @brief Detects Tornado Debris Signature (TDS).
    *
    * @param [in] z Reflectivity (dBZ)
    * @param [in] zdr Differential Reflectivity (dB)
    * @param [in] rhohv Correlation Coefficient (unitless)
    *
    * @return 1.0f if TDS is detected, 0.0f otherwise
    */
   static float DetectTornadoDebris(float z, float zdr, float rhohv);
};

} // namespace wsr88d
} // namespace scwx
