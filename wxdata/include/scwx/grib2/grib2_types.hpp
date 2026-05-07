#pragma once

#include <cstdint>
#include <string>

namespace scwx::grib2
{

enum class Discipline : std::uint8_t
{
   Meteorological = 0,
   Hydrological   = 1,
   LandSurface    = 2,
   Space          = 10,
   Unknown        = 255
};

struct ParameterId
{
   std::uint8_t discipline_;
   std::uint8_t category_;
   std::uint8_t number_;
   std::string  name_;
};

enum class SurfaceType : std::uint8_t
{
   GroundOrWater     = 1,
   CloudBase         = 2,
   CloudTop          = 3,
   Isobaric          = 100,
   MeanSeaLevel      = 101,
   AltitudeAboveMSL  = 102,
   HeightAboveGround = 103,
   Sigma             = 104,
   Hybrid            = 105,
   DepthBelowLand    = 106,
   IsobaricLayer     = 107,
   Unknown           = 255
};

struct GridDef
{
   std::uint32_t ni_;
   std::uint32_t nj_;
   double        latFirst_;
   double        lonFirst_;
   double        latLast_;
   double        lonLast_;
   double        di_;
   double        dj_;
   std::uint8_t  scanningMode_;
};

struct SimplePacking
{
   float        referenceValue_;
   std::int16_t binaryScaleFactor_;
   std::int16_t decimalScaleFactor_;
   std::uint8_t bitsPerValue_;
};

} // namespace scwx::grib2
