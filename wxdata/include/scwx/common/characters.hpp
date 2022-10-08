#pragma once

#include <string>

namespace scwx
{
namespace common
{

namespace Characters
{

constexpr char DEGREE = static_cast<char>(0xb0);
constexpr char ETX    = static_cast<char>(0x03);

} // namespace Characters

namespace Unicode
{

extern const std::string kDegree;

} // namespace Unicode

} // namespace common
} // namespace scwx
