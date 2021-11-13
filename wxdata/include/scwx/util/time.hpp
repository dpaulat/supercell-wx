#pragma once

#include <chrono>

namespace scwx
{
namespace util
{

std::chrono::system_clock::time_point TimePoint(uint16_t modifiedJulianDate,
                                                uint32_t milliseconds);

} // namespace util
} // namespace scwx
