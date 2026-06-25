#include <scwx/qt/render/rhi_vulkan_result.hpp>

#include <scwx/util/logger.hpp>

namespace scwx::qt::render
{

static const std::string logPrefix_ = "scwx::qt::render::rhi_vulkan_result";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static VulkanResultHandler g_handler;

void SetVulkanResultHandler(VulkanResultHandler handler)
{
   g_handler = std::move(handler);
}

void ReportVulkanResult(VkResult result, const char* context)
{
   if (result == VK_SUCCESS)
   {
      return;
   }

   logger_->error("Vulkan call failed in {}: VkResult {}",
                  context,
                  static_cast<int>(result));

   if (g_handler != nullptr)
   {
      g_handler(result, context);
   }
}

} // namespace scwx::qt::render
