#pragma once

class QRhi;
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

   void NewFrame(QWidget* widget);
   void UpdateTextures();
   void RenderDrawData(QRhiCommandBuffer* commandBuffer);

   [[nodiscard]] bool IsInitialized() const;

private:
   bool InitBackend(void* renderPass);

   QRhi* rhi_ {nullptr};
   void* renderPass_ {nullptr};
   bool  initialized_ {false};
};

} // namespace scwx::qt::map
