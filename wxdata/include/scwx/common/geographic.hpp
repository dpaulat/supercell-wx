#pragma once

namespace scwx
{
namespace common
{

struct Coordinate
{
   double latitude_;
   double longitude_;

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
