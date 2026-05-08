#include <scwx/wsr88d/dual_pol_interpreter.hpp>

#include <cmath>

namespace scwx
{
namespace wsr88d
{

DataLevelCode
DualPolInterpreter::ClassifyHydrometeor(float z, float zdr, float rhohv)
{
   if (std::isnan(z) || std::isnan(zdr) || std::isnan(rhohv))
   {
      return DataLevelCode::Unknown;
   }

   // Simplified Fuzzy Logic / Decision Tree for Hydrometeor Classification
   // Based on standard NWS/NSSL criteria

   // 1. Biological (BI) / Non-Meteorological
   if (rhohv < 0.90f && z < 30.0f && zdr > 1.5f)
   {
      return DataLevelCode::Biological;
   }

   // 2. Anomalous Propagation / Ground Clutter (GC)
   if (rhohv < 0.80f)
   {
      return DataLevelCode::AnomalousPropagationGroundClutter;
   }

   // 3. Hail (HA/LH/GH)
   if (z > 50.0f)
   {
      if (rhohv < 0.94f)
      {
         if (z > 60.0f)
            return DataLevelCode::GiantHail;
         if (z > 55.0f)
            return DataLevelCode::LargeHail;
         return DataLevelCode::SmallHail;
      }
      if (zdr < 1.0f && rhohv < 0.97f)
      {
         return DataLevelCode::SmallHail;
      }
   }

   // 4. Graupel (GR)
   if (z >= 35.0f && z < 50.0f && zdr < 1.0f && rhohv > 0.95f && rhohv < 0.99f)
   {
      return DataLevelCode::Graupel;
   }

   // 5. Heavy Rain (HR)
   if (z > 45.0f && zdr > 1.5f && rhohv > 0.95f)
   {
      return DataLevelCode::HeavyRain;
   }

   // 6. Big Drops (BD)
   if (z >= 30.0f && z < 50.0f && zdr > 2.5f && rhohv > 0.92f && rhohv < 0.98f)
   {
      return DataLevelCode::BigDrops;
   }

   // 7. Wet Snow (WS)
   if (z >= 25.0f && z < 45.0f && zdr > 0.5f && zdr < 3.0f && rhohv > 0.90f &&
       rhohv < 0.97f)
   {
      return DataLevelCode::WetSnow;
   }

   // 8. Dry Snow (DS) / Ice Crystals (IC)
   if (z < 35.0f)
   {
      if (rhohv > 0.98f)
      {
         if (zdr > 0.2f && z < 25.0f)
            return DataLevelCode::IceCrystals;
         return DataLevelCode::DrySnow;
      }
   }

   // 9. Light and/or Moderate Rain (RA) - Default for meteorological echoes
   if (z > 0.0f && rhohv > 0.90f)
   {
      return DataLevelCode::LightAndOrModerateRain;
   }

   return DataLevelCode::UnknownClassification;
}

float DualPolInterpreter::DetectTornadoDebris(float z, float zdr, float rhohv)
{
   if (std::isnan(z) || std::isnan(zdr) || std::isnan(rhohv))
   {
      return 0.0f;
   }

   // TDS Detection Criteria
   // 1. High Reflectivity (Z > 35 dBZ)
   // 2. Low Correlation Coefficient (CC < 0.85)
   // 3. Low Differential Reflectivity (ZDR < 0.5 dB)

   if (z > 35.0f && rhohv < 0.85f && zdr < 0.5f)
   {
      // Return 10.0f to match palette indices (e.g., 0=None, 10=Detected)
      return 10.0f;
   }

   return 0.0f;
}

} // namespace wsr88d
} // namespace scwx
