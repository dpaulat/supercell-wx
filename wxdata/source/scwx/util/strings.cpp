#include <scwx/util/strings.hpp>

namespace scwx
{
namespace util
{

std::string ToString(const std::vector<std::string>& v)
{
   std::string value {};

   for (const std::string& s : v)
   {
      if (!value.empty())
      {
         value += ", ";
      }

      value += s;
   }

   return value;
}

} // namespace util
} // namespace scwx
