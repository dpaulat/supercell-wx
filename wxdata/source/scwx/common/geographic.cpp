#include <scwx/common/geographic.hpp>
#include <scwx/common/characters.hpp>

#include <format>

namespace scwx
{
namespace common
{

static std::string GetDegreeString(double             degrees,
                                   DegreeStringType   type,
                                   const std::string& suffix);

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
         std::format("{:.6f}{}{}", degrees, Unicode::kDegree, suffix);
      break;
   case DegreeStringType::DegreesMinutesSeconds:
   {
      uint32_t dd  = static_cast<uint32_t>(degrees);
      degrees      = (degrees - dd) * 60.0;
      uint32_t mm  = static_cast<uint32_t>(degrees);
      double   ss  = (degrees - mm) * 60.0;
      degreeString = std::format(
         "{}{} {}' {:.2f}\"{}", dd, Unicode::kDegree, mm, ss, suffix);
      break;
   }
   }

   return degreeString;
}

} // namespace common
} // namespace scwx
