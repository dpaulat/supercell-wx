#pragma once

class QRhi;
class QRhiCommandBuffer;
class QRhiTexture;
class QWidget;
struct ImGuiContext;

namespace scwx::qt::map
{

// Fenced Metal ImGui path for macOS. Mirror of MapImGuiVulkanRenderer.
class MapImGuiMetalRenderer
{
public:
   void Initialize(QRhi*        rhi,
                   QRhiTexture* colorTexture,
                   void* /* renderPass */,
                   ImGuiContext* imGuiContext);
   void Shutdown();
   void UpdateRenderPass(void* /* renderPass */);

   [[nodiscard]] bool NewFrame(QWidget* widget);
   void               UpdateTextures();
   void               RenderDrawData(QRhiCommandBuffer* commandBuffer);

   [[nodiscard]] bool IsInitialized() const;

private:
   void BindContext() const;
   bool InitBackend(QRhiTexture* colorTexture);
   void EnsureRenderPassDescriptor(QRhiTexture* colorTexture);

   ImGuiContext* imGuiContext_ {nullptr};
   QRhi*         rhi_ {nullptr};
   QRhiTexture*  colorTexture_ {nullptr};
   void*         device_ {nullptr};               // id<MTLDevice>
   void*         renderPassDescriptor_ {nullptr}; // MTLRenderPassDescriptor*
   bool          initialized_ {false};
};

} // namespace scwx::qt::map
