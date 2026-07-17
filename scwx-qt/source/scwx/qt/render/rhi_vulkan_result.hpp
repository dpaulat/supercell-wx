#pragma once

#include <functional>

#if !defined(__APPLE__)
#   include <vulkan/vulkan_core.h>
#endif

namespace scwx::qt::render
{

#if !defined(__APPLE__)
using VulkanResultHandler = std::function<void(VkResult, const char*)>;

void RegisterVulkanResultHandler(const void*         owner,
                                 VulkanResultHandler handler);
void UnregisterVulkanResultHandler(const void* owner);

void ReportVulkanResult(VkResult result, const char* context);
#else
inline void UnregisterVulkanResultHandler(const void* /* owner */) {}
#endif

} // namespace scwx::qt::render
