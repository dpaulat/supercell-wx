#pragma once

#include <cstdint>

namespace scwx
{
namespace util
{

float DecodeFloat16(uint16_t hex);
float DecodeFloat32(uint16_t msw, uint16_t lsw);

} // namespace util
} // namespace scwx
