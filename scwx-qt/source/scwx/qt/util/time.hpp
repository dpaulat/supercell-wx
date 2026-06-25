#pragma once

#include <chrono>

#if (__cpp_lib_chrono < 201907L)
#   include <date/tz.h>
#endif

#include <QDateTime>

namespace scwx
{
namespace qt
{
namespace util
{

#if (__cpp_lib_chrono >= 201907L)
using local_days = std::chrono::local_days;
#else
using local_days = date::local_days;
#endif

/**
 * @brief Convert QDate to std::chrono::sys_days.
 *
 * @param [in] date Date to convert
 *
 * @return Days
 */
std::chrono::sys_days SysDays(const QDate& date);

/**
 * @brief Convert QDate to std::chrono::local_days.
 *
 * @param [in] date Date to convert
 *
 * @return Days
 */
local_days LocalDays(const QDate& date);

} // namespace util
} // namespace qt
} // namespace scwx
