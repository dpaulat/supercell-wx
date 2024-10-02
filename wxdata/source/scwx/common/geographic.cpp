#include <scwx/common/geographic.hpp>
#include <scwx/common/characters.hpp>

#include <numbers>

#include <fmt/format.h>

namespace scwx
{
namespace common
{

static std::string GetDegreeString(double             degrees,
                                   DegreeStringType   type,
                                   const std::string& suffix);

units::degrees<float> GetAngleDelta(units::degrees<float> angle1,
                                    units::degrees<float> angle2)
{
   // Normalize angles to [0, 360)
   while (angle1.value() < 0.0f)
   {
      angle1 += units::degrees<float> {360.0f};
   }
   while (angle2.value() < 0.0f)
   {
      angle2 += units::degrees<float> {360.0f};
   }
   angle1 = units::degrees<float> {std::fmod(angle1.value(), 360.f)};
   angle2 = units::degrees<float> {std::fmod(angle2.value(), 360.f)};

   // Calculate the absolute difference
   auto delta = angle1 - angle2;
   if (delta < units::degrees<float> {0.0f})
   {
      delta *= -1.0f;
   }

   // Account for wrapping
   delta = std::min(delta, units::degrees<float> {360.0f} - delta);

   return delta;
}

Coordinate GetCentroid(const std::vector<Coordinate>& coordinates)
{
   double x = 0.0;
   double y = 0.0;
   double z = 0.0;

   for (const Coordinate& c : coordinates)
   {
      // Convert latitude and longitude to radians
      double latitudeRadians  = c.latitude_ * std::numbers::pi / 180.0;
      double longitudeRadians = c.longitude_ * std::numbers::pi / 180.0;

      // Convert latitude and longitude to Cartesian coordinates
      double x1 = std::cos(latitudeRadians) * std::cos(longitudeRadians);
      double y1 = std::cos(latitudeRadians) * std::sin(longitudeRadians);
      double z1 = std::sin(latitudeRadians);

      // Combine with accumulators
      x += x1;
      y += y1;
      z += z1;
   }

   // Compute averages
   x /= coordinates.size();
   y /= coordinates.size();
   z /= coordinates.size();

   // Convert Cartesian coordinates back to latitude and longitude
   double hyp              = std::sqrt(x * x + y * y);
   double latitudeRadians  = std::atan2(z, hyp);
   double longitudeRadians = std::atan2(y, x);

   // Return latitude and longitude in degrees
   return {latitudeRadians * 180.0 / std::numbers::pi,
           longitudeRadians * 180.0 / std::numbers::pi};
}

std::string GetLatitudeString(double latitude, DegreeStringType type)
{
   std::string suffix {};

   if (latitude > 0.0)
   {
      suffix = " N";
   }
   else if (latitude < 0.0)
   {
      suffix = " S";
   }

   return GetDegreeString(latitude, type, suffix);
}

std::string GetLongitudeString(double longitude, DegreeStringType type)
{
   std::string suffix {};

   if (longitude > 0.0)
   {
      suffix = " E";
   }
   else if (longitude < 0.0)
   {
      suffix = " W";
   }

   return GetDegreeString(longitude, type, suffix);
}

static std::string GetDegreeString(double             degrees,
                                   DegreeStringType   type,
                                   const std::string& suffix)
{
   std::string degreeString {};

   degrees = std::fabs(degrees);

   switch (type)
   {
   case DegreeStringType::Decimal:
      degreeString =
         fmt::format("{:.6f}{}{}", degrees, Unicode::kDegree, suffix);
      break;
   case DegreeStringType::DegreesMinutesSeconds:
   {
      uint32_t dd  = static_cast<uint32_t>(degrees);
      degrees      = (degrees - dd) * 60.0;
      uint32_t mm  = static_cast<uint32_t>(degrees);
      double   ss  = (degrees - mm) * 60.0;
      degreeString = fmt::format(
         "{}{} {}' {:.2f}\"{}", dd, Unicode::kDegree, mm, ss, suffix);
      break;
   }
   }

   return degreeString;
}

} // namespace common
} // namespace scwx
