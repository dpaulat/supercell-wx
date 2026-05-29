#pragma once

#include <string>
#include <vector>

#include <units/angle.h>

namespace scwx
{
namespace common
{

constexpr double kMilesPerMeter      = 0.00062137119;
constexpr double kKilometersPerMeter = 0.001;

constexpr double kDegreesToRadians = 0.0174532925199432957692369055556;

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

/**
 * Calculate the absolute angle delta between two angles.
 *
 * @param [in] angle1 First angle
 * @param [in] angle2 Second angle
 *
 * @return Absolute angle delta normalized to [0, 360)
 */
units::degrees<float> GetAngleDelta(units::degrees<float> angle1,
                                    units::degrees<float> angle2);

/**
 * Calculate the geographic midpoint of a set of coordinates. Uses Method A
 * described at http://www.geomidpoint.com/calculation.html.
 *
 * @param coordinates Set of unique coordinates
 *
 * @return Centroid
 */
Coordinate GetCentroid(const std::vector<Coordinate>& coordinates);

/**
 * Calculate the lat/lon coordinates of a point given a radar's position and
 * the azimuth/range to the point, using the 4/3 Earth radius model.
 *
 * @param radarLatitude Radar latitude in degrees
 * @param radarLongitude Radar longitude in degrees
 * @param radarElevationMeters Radar elevation in meters
 * @param azimuthDegrees Azimuth to the point in degrees
 * @param rangeNauticalMiles Range to the point in nautical miles
 *
 * @return Coordinate of the point
 */
[[nodiscard]] Coordinate polar_to_latlon(double radarLatitude,
                                         double radarLongitude,
                                         double radarElevationMeters,
                                         double azimuthDegrees,
                                         double rangeNauticalMiles);

std::string
GetLatitudeString(double           latitude,
                  DegreeStringType type = DegreeStringType::Decimal);
std::string
GetLongitudeString(double           longitude,
                   DegreeStringType type = DegreeStringType::Decimal);

} // namespace common
} // namespace scwx
