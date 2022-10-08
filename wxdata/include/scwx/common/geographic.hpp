#pragma once

#include <string>

namespace scwx
{
namespace common
{

constexpr double kMilesPerMeter      = 0.00062137119;
constexpr double kKilometersPerMeter = 0.001;

/**
 * @brief Coordinate type to hold latitude and longitude of a location.
 */
struct Coordinate
{
   double latitude_;  ///< Latitude in degrees
   double longitude_; ///< Longitude in degrees

   Coordinate() : Coordinate(0.0, 0.0) {}

   Coordinate(double latitude, double longitude) :
       latitude_ {latitude}, longitude_ {longitude}
   {
   }

   bool operator==(const Coordinate& o) const
   {
      return latitude_ == o.latitude_ && longitude_ == o.longitude_;
   }
};

enum class DegreeStringType
{
   Decimal,
   DegreesMinutesSeconds
};

enum class DistanceType
{
   Kilometers,
   Miles
};

std::string
GetLatitudeString(double           latitude,
                  DegreeStringType type = DegreeStringType::Decimal);
std::string
GetLongitudeString(double           longitude,
                   DegreeStringType type = DegreeStringType::Decimal);

} // namespace common
} // namespace scwx
