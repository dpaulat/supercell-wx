#pragma once

#include <chrono>

namespace scwx
{
namespace util
{

std::chrono::system_clock::time_point TimePoint(uint32_t modifiedJulianDate,
                                                uint32_t milliseconds);

std::string TimeString(std::chrono::system_clock::time_point time,
                       const std::chrono::time_zone*         timeZone = nullptr,
                       bool                                  epochValid = true);

} // namespace util
} // namespace scwx
