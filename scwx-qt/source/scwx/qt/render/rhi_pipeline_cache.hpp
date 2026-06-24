#pragma once

#include <QByteArray>

class QRhi;

#include <vulkan/vulkan_core.h>

namespace scwx::qt::render
{

void RestoreQrhiPipelineCache(QRhi* rhi);
void PersistQrhiPipelineCache(QRhi* rhi);

[[nodiscard]] QByteArray LoadVulkanPipelineCacheBlob(const char* fileName);
void                     SaveVulkanPipelineCacheBlob(const char* fileName,
                                                     const QByteArray& data);

[[nodiscard]] VkResult CreateVulkanPipelineCache(
   VkDevice                 device,
   const QByteArray&        initialData,
   VkPipelineCache*         pipelineCache);
[[nodiscard]] QByteArray GetVulkanPipelineCacheBlob(VkDevice          device,
                                                    VkPipelineCache   pipelineCache);

} // namespace scwx::qt::render
