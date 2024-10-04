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
   MarkerInfo(std::string name, double latitude, double longitude) :
       name_ {name}, latitude_ {latitude}, longitude_ {longitude}
   {
   }

   std::string name_;
   double      latitude_;
   double      longitude_;
};

} // namespace types
} // namespace qt
} // namespace scwx
