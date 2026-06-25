#include <scwx/qt/map/map_imgui_vulkan_renderer.hpp>
#include <scwx/qt/model/imgui_context_model.hpp>
#include <scwx/qt/render/rhi_imgui_util.hpp>
#include <scwx/qt/render/rhi_pipeline_cache.hpp>
#include <scwx/qt/render/rhi_vulkan_result.hpp>
#include <scwx/util/logger.hpp>

#include <backends/imgui_impl_qt.hpp>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

namespace scwx::qt::map
{

static const std::string logPrefix_ =
   "scwx::qt::map::map_imgui_vulkan_renderer";
static const auto logger_ = scwx::util::Logger::Create(logPrefix_);

static const char kImGuiPipelineCacheFile[] = "imgui-vulkan-pipeline-cache.bin";

static void CheckVkResult(VkResult result)
{
   render::ReportVulkanResult(result, "ImGui Vulkan");
}

bool MapImGuiVulkanRenderer::InitBackend(void* renderPass)
{
   if (rhi_ == nullptr || renderPass == nullptr ||
       rhi_->backend() != QRhi::Vulkan)
   {
      return false;
   }

   const QRhiNativeHandles* nativeHandles = rhi_->nativeHandles();
   if (nativeHandles == nullptr)
   {
      return false;
   }

   const auto* vkHandles =
      static_cast<const QRhiVulkanNativeHandles*>(nativeHandles);
   if (vkHandles->inst == nullptr || vkHandles->physDev == VK_NULL_HANDLE ||
       vkHandles->dev == VK_NULL_HANDLE ||
       vkHandles->gfxQueue == VK_NULL_HANDLE)
   {
      return false;
   }

   ImGui_ImplVulkan_InitInfo initInfo {};
   initInfo.ApiVersion                  = VK_API_VERSION_1_3;
   initInfo.Instance                    = vkHandles->inst->vkInstance();
   initInfo.PhysicalDevice              = vkHandles->physDev;
   initInfo.Device                      = vkHandles->dev;
   initInfo.QueueFamily                 = vkHandles->gfxQueueFamilyIdx;
   initInfo.Queue                       = vkHandles->gfxQueue;
   initInfo.PipelineInfoMain.RenderPass = static_cast<VkRenderPass>(renderPass);
   initInfo.PipelineInfoMain.Subpass    = 0;
   initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
   initInfo.MinImageCount                = 2;
   initInfo.ImageCount                   = 2;
   initInfo.DescriptorPoolSize =
      IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE * 16;
   initInfo.UseDynamicRendering = false;
   initInfo.CheckVkResultFn     = CheckVkResult;
   initInfo.MinAllocationSize   = static_cast<VkDeviceSize>(1024) * 1024;

   const QByteArray pipelineCacheBlob =
      render::LoadVulkanPipelineCacheBlob(kImGuiPipelineCacheFile);
   VkPipelineCache pipelineCache = VK_NULL_HANDLE;
   if (render::CreateVulkanPipelineCache(
          vkHandles->dev, pipelineCacheBlob, &pipelineCache) != VK_SUCCESS)
   {
      logger_->error("Failed to create ImGui Vulkan pipeline cache");
      return false;
   }

   initInfo.PipelineCache = pipelineCache;

   if (!ImGui_ImplVulkan_Init(&initInfo))
   {
      logger_->error("ImGui_ImplVulkan_Init failed");
      vkDestroyPipelineCache(vkHandles->dev, pipelineCache, nullptr);
      return false;
   }

   device_        = vkHandles->dev;
   pipelineCache_ = pipelineCache;

   return true;
}

void MapImGuiVulkanRenderer::Initialize(QRhi* rhi,
                                        QRhiTexture* /* colorTexture */,
                                        void* renderPass)
{
   if (initialized_ || rhi == nullptr || renderPass == nullptr)
   {
      return;
   }

   if (ImGui::GetCurrentContext() == nullptr)
   {
      return;
   }

   if (ImGui::GetIO().BackendRendererUserData != nullptr)
   {
      logger_->warn(
         "ImGui Vulkan backend already bound to current context; skipping "
         "init");
      return;
   }

   rhi_        = rhi;
   renderPass_ = renderPass;

   if (!InitBackend(renderPass))
   {
      rhi_        = nullptr;
      renderPass_ = nullptr;
      return;
   }

   initialized_ = true;
   logger_->debug("ImGui Vulkan renderer initialized");
}

void MapImGuiVulkanRenderer::UpdateRenderPass(void* renderPass)
{
   if (rhi_ == nullptr || renderPass == nullptr || renderPass == renderPass_)
   {
      return;
   }

   if (initialized_)
   {
      Shutdown();
   }

   renderPass_ = renderPass;

   if (!InitBackend(renderPass))
   {
      renderPass_ = nullptr;
      return;
   }

   initialized_ = true;
   logger_->debug("ImGui Vulkan renderer reinitialized for new render pass");
}

void MapImGuiVulkanRenderer::Shutdown()
{
   if (initialized_)
   {
      if (pipelineCache_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE)
      {
         const QByteArray pipelineCacheBlob =
            render::GetVulkanPipelineCacheBlob(device_, pipelineCache_);
         render::SaveVulkanPipelineCacheBlob(kImGuiPipelineCacheFile,
                                             pipelineCacheBlob);
         vkDestroyPipelineCache(device_, pipelineCache_, nullptr);
         pipelineCache_ = VK_NULL_HANDLE;
         device_        = VK_NULL_HANDLE;
      }

      if (ImGui::GetCurrentContext() != nullptr &&
          ImGui::GetIO().BackendRendererUserData != nullptr)
      {
         ImGui_ImplVulkan_Shutdown();
      }
      initialized_ = false;
   }

   rhi_        = nullptr;
   renderPass_ = nullptr;
}

bool MapImGuiVulkanRenderer::NewFrame(QWidget* widget)
{
   if (!initialized_ || widget == nullptr)
   {
      return false;
   }

   model::ImGuiContextModel::Instance().NewFrame();
   ImGui_ImplQt_NewFrame(widget);
   ImGui_ImplVulkan_NewFrame();
   ImGui::NewFrame();
   return true;
}

void MapImGuiVulkanRenderer::UpdateTextures()
{
   if (!initialized_)
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
         ImGui_ImplVulkan_UpdateTexture(texture);
      }
   }
}

void MapImGuiVulkanRenderer::RenderDrawData(QRhiCommandBuffer* commandBuffer)
{
   if (!initialized_ || commandBuffer == nullptr)
   {
      return;
   }

   render::RenderImGuiDrawData(commandBuffer);
}

bool MapImGuiVulkanRenderer::IsInitialized() const
{
   return initialized_;
}

} // namespace scwx::qt::map
