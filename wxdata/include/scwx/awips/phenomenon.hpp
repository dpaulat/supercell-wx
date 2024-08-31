#pragma once

#include <string>

namespace scwx
{
namespace awips
{

enum class Phenomenon
{
   AshfallLand,
   AirStagnation,
   BeachHazard,
   BriskWind,
   Blizzard,
   CoastalFlood,
   DebrisFlow,
   DustStorm,
   BlowingDust,
   ExtremeCold,
   ExcessiveHeat,
   ExtremeWind,
   Flood,
   FlashFlood,
   DenseFogLand,
   FloodForecastPoints,
   Frost,
   FireWeather,
   Freeze,
   Gale,
   HurricaneForceWind,
   Heat,
   Hurricane,
   HighWind,
   Hydrologic,
   HardFreeze,
   IceStorm,
   LakeEffectSnow,
   LowWater,
   LakeshoreFlood,
   LakeWind,
   Marine,
   DenseFogMarine,
   AshfallMarine,
   DenseSmokeMarine,
   RipCurrentRisk,
   SmallCraft,
   HazardousSeas,
   DenseSmokeLand,
   Storm,
   StormSurge,
   SnowSquall,
   HighSurf,
   SevereThunderstorm,
   Tornado,
   TropicalStorm,
   Tsunami,
   Typhoon,
   HeavyFreezingSpray,
   WindChill,
   Wind,
   WinterStorm,
   WinterWeather,
   FreezingFog,
   FreezingRain,
   FreezingSpray,
   Unknown
};

Phenomenon         GetPhenomenon(const std::string& code);
Phenomenon         GetPhenomenonFromText(const std::string& text);
const std::string& GetPhenomenonCode(Phenomenon phenomenon);
const std::string& GetPhenomenonText(Phenomenon phenomenon);

} // namespace awips
} // namespace scwx
