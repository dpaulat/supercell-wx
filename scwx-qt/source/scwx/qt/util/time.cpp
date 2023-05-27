#include <scwx/qt/util/time.hpp>

namespace scwx
{
namespace qt
{
namespace util
{

std::chrono::sys_days SysDays(const QDate& date)
{
   using namespace std::chrono;
   using sys_days             = time_point<system_clock, days>;
   constexpr auto julianEpoch = sys_days {-4713y / November / 24d};
   constexpr auto unixEpoch   = sys_days {1970y / January / 1d};
   constexpr auto offset      = std::chrono::days(julianEpoch - unixEpoch);

   return std::chrono::sys_days(std::chrono::days(date.toJulianDay()) +
                                julianEpoch);
}

} // namespace util
} // namespace qt
} // namespace scwx
