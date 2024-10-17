#pragma once

#include <string>

namespace scwx
{
namespace qt
{
namespace types
{

struct MarkerInfo
{
   MarkerInfo(const std::string& name, double latitude, double longitude) :
       name {name}, latitude {latitude}, longitude {longitude}
   {
   }

   std::string name;
   double      latitude;
   double      longitude;
};

} // namespace types
} // namespace qt
} // namespace scwx
