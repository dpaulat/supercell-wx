#pragma once

namespace scwx
{
namespace common
{

/**
 * @brief Coordinate type to hold latitude and longitude of a location.
 */
struct Coordinate
{
   double latitude_;  ///< Latitude in degrees
   double longitude_; ///< Longitude in degrees

   Coordinate(double latitude, double longitude) :
       latitude_ {latitude}, longitude_ {longitude}
   {
   }

   bool operator==(const Coordinate& o) const
   {
      return latitude_ == o.latitude_ && longitude_ == o.longitude_;
   }
};

} // namespace common
} // namespace scwx
