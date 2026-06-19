#pragma once

#include <cstdint>

namespace scwx::qt::types
{

enum class CaptureType : std::uint8_t
{
   Copy,
   SaveImage,
   None
};

} // namespace scwx::qt::types
