#include <scwx/util/time.hpp>

namespace scwx
{
namespace util
{

std::chrono::system_clock::time_point
TimePoint(uint16_t modifiedJulianDate, uint32_t milliseconds)
{
   using namespace std::chrono;
   using sys_days       = time_point<system_clock, days>;
   constexpr auto epoch = sys_days {1969y / December / 31d};

   return epoch + (modifiedJulianDate * 24h) +
          std::chrono::milliseconds {milliseconds};
}

} // namespace qt
} // namespace scwx
