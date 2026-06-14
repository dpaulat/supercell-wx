#pragma once

#include <fmt/format.h>

#include <stdexcept>
#include <vulkan/vulkan.h>

namespace scwx::qt::vk
{

inline void CheckResult(VkResult           result,
                        const char*        expression,
                        const char*        file,
                        int                line)
{
   if (result != VK_SUCCESS)
   {
      throw std::runtime_error(
         fmt::format("{} failed with VkResult {} at {}:{}",
                     expression,
                     static_cast<int>(result),
                     file,
                     line));
   }
}

} // namespace scwx::qt::vk

#define SCWX_VK_CHECK(expr) \
   scwx::qt::vk::CheckResult((expr), #expr, __FILE__, __LINE__)
