#pragma once

#include <chrono>

#include <QDateTime>

namespace scwx
{
namespace qt
{
namespace util
{

/**
 * @brief Convert QDate to std::chrono::sys_days.
 *
 * @param [in] date Date to convert
 *
 * @return Days
 */
std::chrono::sys_days SysDays(const QDate& date);

} // namespace util
} // namespace qt
} // namespace scwx
