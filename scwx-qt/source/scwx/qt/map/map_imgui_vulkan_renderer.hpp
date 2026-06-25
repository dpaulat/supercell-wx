#pragma once

class QRhi;
class QRhiCommandBuffer;
class QRhiTexture;
class QWidget;

#include <vulkan/vulkan_core.h>
class QRhiCommandBuffer;
class QRhiTexture;
class QWidget;

namespace scwx::qt::map
{

class MapImGuiVulkanRenderer
{
public:
   void Initialize(QRhi* rhi, QRhiTexture* colorTexture, void* renderPass);
   void Shutdown();
   void UpdateRenderPass(void* renderPass);

   [[nodiscard]] bool NewFrame(QWidget* widget);
   void               UpdateTextures();
   void               RenderDrawData(QRhiCommandBuffer* commandBuffer);

   [[nodiscard]] bool IsInitialized() const;

private:
   bool InitBackend(void* renderPass);

   QRhi*           rhi_ {nullptr};
   void*           renderPass_ {nullptr};
   bool            initialized_ {false};
   VkDevice        device_ {VK_NULL_HANDLE};
   VkPipelineCache pipelineCache_ {VK_NULL_HANDLE};
};

} // namespace scwx::qt::map
