#include <scwx/awips/phenomenon.hpp>
#include <scwx/util/logger.hpp>

#include <boost/assign.hpp>
#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>

namespace scwx
{
namespace awips
{

static const std::string logPrefix_ = "scwx::awips::phenomenon";
static const auto        logger_    = util::Logger::Create(logPrefix_);

typedef boost::bimap<boost::bimaps::unordered_set_of<Phenomenon>,
                     boost::bimaps::unordered_set_of<std::string>>
   PhenomenonCodesBimap;

static const PhenomenonCodesBimap phenomenonCodes_ =
   boost::assign::list_of<PhenomenonCodesBimap::relation> //
   (Phenomenon::AshfallLand, "AF")                        //
   (Phenomenon::AirStagnation, "AS")                      //
   (Phenomenon::BeachHazard, "BH")                        //
   (Phenomenon::BriskWind, "BW")                          //
   (Phenomenon::Blizzard, "BZ")                           //
   (Phenomenon::CoastalFlood, "CF")                       //
   (Phenomenon::DebrisFlow, "DF")                         //
   (Phenomenon::DustStorm, "DS")                          //
   (Phenomenon::BlowingDust, "DU")                        //
   (Phenomenon::ExtremeCold, "EC")                        //
   (Phenomenon::ExcessiveHeat, "EH")                      //
   (Phenomenon::ExtremeWind, "EW")                        //
   (Phenomenon::Flood, "FA")                              //
   (Phenomenon::FlashFlood, "FF")                         //
   (Phenomenon::DenseFogLand, "FG")                       //
   (Phenomenon::FloodForecastPoints, "FL")                //
   (Phenomenon::Frost, "FR")                              //
   (Phenomenon::FireWeather, "FW")                        //
   (Phenomenon::Freeze, "FZ")                             //
   (Phenomenon::Gale, "GL")                               //
   (Phenomenon::HurricaneForceWind, "HF")                 //
   (Phenomenon::Heat, "HT")                               //
   (Phenomenon::Hurricane, "HU")                          //
   (Phenomenon::HighWind, "HW")                           //
   (Phenomenon::Hydrologic, "HY")                         //
   (Phenomenon::HardFreeze, "HZ")                         //
   (Phenomenon::IceStorm, "IS")                           //
   (Phenomenon::LakeEffectSnow, "LE")                     //
   (Phenomenon::LowWater, "LO")                           //
   (Phenomenon::LakeshoreFlood, "LS")                     //
   (Phenomenon::LakeWind, "LW")                           //
   (Phenomenon::Marine, "MA")                             //
   (Phenomenon::DenseFogMarine, "MF")                     //
   (Phenomenon::AshfallMarine, "MH")                      //
   (Phenomenon::DenseSmokeMarine, "MS")                   //
   (Phenomenon::RipCurrentRisk, "RP")                     //
   (Phenomenon::SmallCraft, "SC")                         //
   (Phenomenon::HazardousSeas, "SE")                      //
   (Phenomenon::DenseSmokeLand, "SM")                     //
   (Phenomenon::Storm, "SR")                              //
   (Phenomenon::StormSurge, "SS")                         //
   (Phenomenon::SnowSquall, "SQ")                         //
   (Phenomenon::HighSurf, "SU")                           //
   (Phenomenon::SevereThunderstorm, "SV")                 //
   (Phenomenon::Tornado, "TO")                            //
   (Phenomenon::TropicalStorm, "TR")                      //
   (Phenomenon::Tsunami, "TS")                            //
   (Phenomenon::Typhoon, "TY")                            //
   (Phenomenon::HeavyFreezingSpray, "UP")                 //
   (Phenomenon::WindChill, "WC")                          //
   (Phenomenon::Wind, "WI")                               //
   (Phenomenon::WinterStorm, "WS")                        //
   (Phenomenon::WinterWeather, "WW")                      //
   (Phenomenon::FreezingFog, "ZF")                        //
   (Phenomenon::FreezingRain, "ZR")                       //
   (Phenomenon::FreezingSpray, "ZY")                      //
   (Phenomenon::Unknown, "??");

static const PhenomenonCodesBimap phenomenonText_ =
   boost::assign::list_of<PhenomenonCodesBimap::relation>   //
   (Phenomenon::AshfallLand, "Ashfall (land)")              //
   (Phenomenon::AirStagnation, "Air Stagnation")            //
   (Phenomenon::BeachHazard, "Beach Hazard")                //
   (Phenomenon::BriskWind, "Brisk Wind")                    //
   (Phenomenon::Blizzard, "Blizzard")                       //
   (Phenomenon::CoastalFlood, "Coastal Flood")              //
   (Phenomenon::DebrisFlow, "Debris Flow")                  //
   (Phenomenon::DustStorm, "Dust Storm")                    //
   (Phenomenon::BlowingDust, "Blowing Dust")                //
   (Phenomenon::ExtremeCold, "Extreme Cold")                //
   (Phenomenon::ExcessiveHeat, "Excessive Heat")            //
   (Phenomenon::ExtremeWind, "Extreme Wind")                //
   (Phenomenon::Flood, "Flood")                             //
   (Phenomenon::FlashFlood, "Flash Flood")                  //
   (Phenomenon::DenseFogLand, "Dense Fog (land)")           //
   (Phenomenon::Flood, "Flood (Forecast Points)")           //
   (Phenomenon::Frost, "Frost")                             //
   (Phenomenon::FireWeather, "Fire Weather")                //
   (Phenomenon::Freeze, "Freeze")                           //
   (Phenomenon::Gale, "Gale")                               //
   (Phenomenon::HurricaneForceWind, "Hurricane Force Wind") //
   (Phenomenon::Heat, "Heat")                               //
   (Phenomenon::Hurricane, "Hurricane")                     //
   (Phenomenon::HighWind, "High Wind")                      //
   (Phenomenon::Hydrologic, "Hydrologic")                   //
   (Phenomenon::HardFreeze, "Hard Freeze")                  //
   (Phenomenon::IceStorm, "Ice Storm")                      //
   (Phenomenon::LakeEffectSnow, "Lake Effect Snow")         //
   (Phenomenon::LowWater, "Low Water")                      //
   (Phenomenon::LakeshoreFlood, "Lakeshore Flood")          //
   (Phenomenon::LakeWind, "Lake Wind")                      //
   (Phenomenon::Marine, "Marine")                           //
   (Phenomenon::DenseFogMarine, "Dense Fog (marine)")       //
   (Phenomenon::AshfallMarine, "Ashfall (marine)")          //
   (Phenomenon::DenseSmokeMarine, "Dense Smoke (marine)")   //
   (Phenomenon::RipCurrentRisk, "Rip Current Risk")         //
   (Phenomenon::SmallCraft, "Small Craft")                  //
   (Phenomenon::HazardousSeas, "Hazardous Seas")            //
   (Phenomenon::DenseSmokeLand, "Dense Smoke (land)")       //
   (Phenomenon::Storm, "Storm")                             //
   (Phenomenon::StormSurge, "Storm Surge")                  //
   (Phenomenon::SnowSquall, "Snow Squall")                  //
   (Phenomenon::HighSurf, "High Surf")                      //
   (Phenomenon::SevereThunderstorm, "Severe Thunderstorm")  //
   (Phenomenon::Tornado, "Tornado")                         //
   (Phenomenon::TropicalStorm, "Tropical Storm")            //
   (Phenomenon::Tsunami, "Tsunami")                         //
   (Phenomenon::Typhoon, "Typhoon")                         //
   (Phenomenon::HeavyFreezingSpray, "Heavy Freezing Spray") //
   (Phenomenon::WindChill, "Wind Chill")                    //
   (Phenomenon::Wind, "Wind")                               //
   (Phenomenon::WinterStorm, "Winter Storm")                //
   (Phenomenon::WinterWeather, "Winter Weather")            //
   (Phenomenon::FreezingFog, "Freezing Fog")                //
   (Phenomenon::FreezingRain, "Freezing Rain")              //
   (Phenomenon::FreezingSpray, "Freezing Spray")            //
   (Phenomenon::Unknown, "Unknown");

Phenomenon GetPhenomenon(const std::string& code)
{
   Phenomenon phenomenon;

   if (phenomenonCodes_.right.find(code) != phenomenonCodes_.right.end())
   {
      phenomenon = phenomenonCodes_.right.at(code);
   }
   else
   {
      phenomenon = Phenomenon::Unknown;

      logger_->debug("Unrecognized code: \"{}\"", code);
   }

   return phenomenon;
}

Phenomenon GetPhenomenonFromText(const std::string& text)
{
   Phenomenon phenomenon;

   if (phenomenonText_.right.find(text) != phenomenonText_.right.end())
   {
      phenomenon = phenomenonText_.right.at(text);
   }
   else
   {
      phenomenon = Phenomenon::Unknown;

      logger_->debug("Unrecognized code: \"{}\"", text);
   }

   return phenomenon;
}

const std::string& GetPhenomenonCode(Phenomenon phenomenon)
{
   return phenomenonCodes_.left.at(phenomenon);
}

const std::string& GetPhenomenonText(Phenomenon phenomenon)
{
   return phenomenonText_.left.at(phenomenon);
}

} // namespace awips
} // namespace scwx
