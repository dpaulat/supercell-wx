#include <scwx/qt/map/map_imgui_metal_renderer.hpp>
#include <scwx/qt/model/imgui_context_model.hpp>
#include <scwx/util/logger.hpp>

#include <backends/imgui_impl_qt.hpp>
#include <backends/imgui_impl_metal.h>
#include <imgui.h>

#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

#import <Metal/Metal.h>
#import <CoreFoundation/CoreFoundation.h>

#include <cstdint>

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::map_imgui_metal_renderer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

void MapImGuiMetalRenderer::BindContext() const
{
   if (imGuiContext_ != nullptr)
   {
      ImGui::SetCurrentContext(imGuiContext_);
   }
}

void MapImGuiMetalRenderer::EnsureRenderPassDescriptor(QRhiTexture* colorTexture)
{
   if (colorTexture == nullptr)
   {
      return;
   }

   MTLRenderPassDescriptor* desc =
      (__bridge MTLRenderPassDescriptor*) renderPassDescriptor_;
   if (desc == nil)
   {
      desc = [MTLRenderPassDescriptor renderPassDescriptor];
      renderPassDescriptor_ = (__bridge_retained void*) desc;
   }

   // ImGui reads pixelFormat from the attachment texture. Qt's Metal native
   // handle is MTLTexture* stuffed into NativeTexture::object (quint64).
   id<MTLTexture> texture = (__bridge id<MTLTexture>)(void*)(uintptr_t)
                               colorTexture->nativeTexture().object;

   desc.colorAttachments[0].texture     = texture;
   desc.colorAttachments[0].loadAction  = MTLLoadActionLoad;
   desc.colorAttachments[0].storeAction = MTLStoreActionStore;
}

bool MapImGuiMetalRenderer::InitBackend(QRhiTexture* colorTexture)
{
   if (rhi_ == nullptr || rhi_->backend() != QRhi::Metal)
   {
      return false;
   }

   BindContext();
   if (ImGui::GetCurrentContext() == nullptr)
   {
      return false;
   }

   if (ImGui::GetIO().BackendRendererUserData != nullptr)
   {
      logger_->warn("Clearing stale ImGui Metal backend before re-init");
      ImGui_ImplMetal_Shutdown();
   }

   const QRhiNativeHandles* nativeHandles = rhi_->nativeHandles();
   if (nativeHandles == nullptr)
   {
      return false;
   }

   const auto* metalHandles =
      static_cast<const QRhiMetalNativeHandles*>(nativeHandles);
   // Qt forward-declares MTLDevice as a class; Metal.h uses a protocol.
   // ARC: ObjC* → void* needs __bridge, then void* → id<>.
   id<MTLDevice> device =
      (__bridge id<MTLDevice>)(__bridge void*) metalHandles->dev;
   if (device == nil)
   {
      return false;
   }

   if (!ImGui_ImplMetal_Init(device))
   {
      logger_->error("ImGui_ImplMetal_Init failed");
      return false;
   }

   device_ = (__bridge void*) metalHandles->dev;
   EnsureRenderPassDescriptor(colorTexture);
   return true;
}

void MapImGuiMetalRenderer::Initialize(QRhi*         rhi,
                                       QRhiTexture*  colorTexture,
                                       void* /* renderPass */,
                                       ImGuiContext* imGuiContext)
{
   if (rhi == nullptr)
   {
      return;
   }

   imGuiContext_ = imGuiContext;
   BindContext();

   if (ImGui::GetCurrentContext() == nullptr)
   {
      return;
   }

   if (initialized_)
   {
      if (rhi_ == rhi && colorTexture_ == colorTexture)
      {
         return;
      }
      Shutdown();
      imGuiContext_ = imGuiContext;
      BindContext();
   }

   rhi_          = rhi;
   colorTexture_ = colorTexture;

   if (!InitBackend(colorTexture))
   {
      rhi_          = nullptr;
      colorTexture_ = nullptr;
      return;
   }

   initialized_ = true;
   logger_->debug("ImGui Metal renderer initialized");
}

void MapImGuiMetalRenderer::UpdateRenderPass(void* /* renderPass */)
{
   // Metal ImGui keys off pixel format via EnsureRenderPassDescriptor, not a
   // VkRenderPass handle. Re-init if the color texture format changed.
   if (!initialized_ || colorTexture_ == nullptr)
   {
      return;
   }
   EnsureRenderPassDescriptor(colorTexture_);
}

void MapImGuiMetalRenderer::Shutdown()
{
   BindContext();

   if (initialized_)
   {
      if (ImGui::GetCurrentContext() != nullptr &&
          ImGui::GetIO().BackendRendererUserData != nullptr)
      {
         ImGui_ImplMetal_Shutdown();
      }
      initialized_ = false;
   }
   else if (ImGui::GetCurrentContext() != nullptr &&
            ImGui::GetIO().BackendRendererUserData != nullptr)
   {
      logger_->warn("Shutting down orphaned ImGui Metal backend");
      ImGui_ImplMetal_Shutdown();
   }

   if (renderPassDescriptor_ != nullptr)
   {
      CFRelease(renderPassDescriptor_);
      renderPassDescriptor_ = nullptr;
   }

   rhi_          = nullptr;
   colorTexture_ = nullptr;
   device_       = nullptr;
}

bool MapImGuiMetalRenderer::NewFrame(QWidget* widget)
{
   if (!initialized_ || widget == nullptr)
   {
      return false;
   }

   BindContext();
   if (ImGui::GetCurrentContext() == nullptr ||
       ImGui::GetIO().BackendRendererUserData == nullptr)
   {
      logger_->error("ImGui Metal NewFrame without backend; forcing shutdown");
      initialized_ = false;
      return false;
   }

   EnsureRenderPassDescriptor(colorTexture_);
   MTLRenderPassDescriptor* desc =
      (__bridge MTLRenderPassDescriptor*) renderPassDescriptor_;

   model::ImGuiContextModel::Instance().NewFrame();
   ImGui_ImplQt_NewFrame(widget);
   ImGui_ImplMetal_NewFrame(desc);
   ImGui::NewFrame();
   return true;
}

void MapImGuiMetalRenderer::UpdateTextures()
{
   if (!initialized_)
   {
      return;
   }

   BindContext();
   if (ImGui::GetCurrentContext() == nullptr ||
       ImGui::GetIO().BackendRendererUserData == nullptr)
   {
      return;
   }

   ImDrawData* drawData = ImGui::GetDrawData();
   if (drawData == nullptr || drawData->Textures == nullptr)
   {
      return;
   }

   for (ImTextureData* texture : *drawData->Textures)
   {
      if (texture != nullptr && texture->Status != ImTextureStatus_OK)
      {
         ImGui_ImplMetal_UpdateTexture(texture);
      }
   }
}

void MapImGuiMetalRenderer::RenderDrawData(QRhiCommandBuffer* commandBuffer)
{
   if (!initialized_ || commandBuffer == nullptr)
   {
      return;
   }

   BindContext();
   if (ImGui::GetCurrentContext() == nullptr ||
       ImGui::GetIO().BackendRendererUserData == nullptr)
   {
      return;
   }

   ImDrawData* drawData = ImGui::GetDrawData();
   if (drawData == nullptr || drawData->TotalVtxCount <= 0)
   {
      return;
   }

   const QRhiNativeHandles* nativeHandles = commandBuffer->nativeHandles();
   if (nativeHandles == nullptr)
   {
      return;
   }

   const auto* metalHandles =
      static_cast<const QRhiMetalCommandBufferNativeHandles*>(nativeHandles);
   id<MTLCommandBuffer> commandBufferMetal =
      (__bridge id<MTLCommandBuffer>)(__bridge void*)
         metalHandles->commandBuffer;
   id<MTLRenderCommandEncoder> encoder =
      (__bridge id<MTLRenderCommandEncoder>)(__bridge void*)
         metalHandles->encoder;
   if (commandBufferMetal == nil || encoder == nil)
   {
      return;
   }

   ImGui_ImplMetal_RenderDrawData(drawData, commandBufferMetal, encoder);
}

bool MapImGuiMetalRenderer::IsInitialized() const
{
   return initialized_;
}

} // namespace scwx::qt::map
