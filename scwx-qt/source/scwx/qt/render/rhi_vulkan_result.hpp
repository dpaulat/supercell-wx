#pragma once

#include <functional>
#include <vulkan/vulkan_core.h>

namespace scwx::qt::render
{

using VulkanResultHandler = std::function<void(VkResult, const char*)>;

void SetVulkanResultHandler(VulkanResultHandler handler);
void ReportVulkanResult(VkResult result, const char* context);

} // namespace scwx::qt::render
