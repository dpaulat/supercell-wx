#pragma once

#include <QByteArray>

class QRhi;

#if !defined(__APPLE__)
#   include <vulkan/vulkan_core.h>
#endif

namespace scwx::qt::render
{

void RestoreQrhiPipelineCache(QRhi* rhi);
void PersistQrhiPipelineCache(QRhi* rhi);

#if !defined(__APPLE__)
[[nodiscard]] QByteArray LoadVulkanPipelineCacheBlob(const char* fileName);
void SaveVulkanPipelineCacheBlob(const char* fileName, const QByteArray& data);

[[nodiscard]] VkResult
CreateVulkanPipelineCache(VkDevice          device,
                          const QByteArray& initialData,
                          VkPipelineCache*  pipelineCache);
[[nodiscard]] QByteArray
GetVulkanPipelineCacheBlob(VkDevice device, VkPipelineCache pipelineCache);
#endif

} // namespace scwx::qt::render
