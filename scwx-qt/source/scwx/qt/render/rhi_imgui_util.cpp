#include <scwx/qt/render/rhi_imgui_util.hpp>

#include <imgui.h>

#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

#if !defined(__APPLE__)
#   include <backends/imgui_impl_vulkan.h>
#endif

namespace scwx::qt::render
{

void RenderImGuiDrawData(QRhiCommandBuffer* commandBuffer)
{
#if defined(__APPLE__)
   // Metal path renders from MapImGuiMetalRenderer (ObjC++).
   Q_UNUSED(commandBuffer);
#else
   if (commandBuffer == nullptr)
   {
      return;
   }

   ImDrawData* drawData = ImGui::GetDrawData();
   if (drawData == nullptr || drawData->TotalVtxCount <= 0)
   {
      return;
   }

   if (ImGui::GetCurrentContext() == nullptr ||
       ImGui::GetIO().BackendRendererUserData == nullptr)
   {
      return;
   }

   const QRhiNativeHandles* nativeHandles = commandBuffer->nativeHandles();
   if (nativeHandles == nullptr)
   {
      return;
   }

   const auto* vkHandles =
      static_cast<const QRhiVulkanCommandBufferNativeHandles*>(nativeHandles);
   if (vkHandles->commandBuffer == VK_NULL_HANDLE)
   {
      return;
   }

   ImGui_ImplVulkan_RenderDrawData(drawData, vkHandles->commandBuffer);
#endif
}

} // namespace scwx::qt::render
