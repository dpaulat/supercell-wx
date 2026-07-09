#include <scwx/qt/render/rhi_vulkan_result.hpp>

#include <scwx/util/logger.hpp>

#include <mutex>
#include <unordered_map>

namespace scwx::qt::render
{

static const std::string logPrefix_ = "scwx::qt::render::rhi_vulkan_result";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static std::mutex                                           g_handlerMutex;
static std::unordered_map<const void*, VulkanResultHandler> g_handlers;

void RegisterVulkanResultHandler(const void* owner, VulkanResultHandler handler)
{
   if (owner == nullptr)
   {
      return;
   }

   std::lock_guard lock {g_handlerMutex};
   g_handlers[owner] = std::move(handler);
}

void UnregisterVulkanResultHandler(const void* owner)
{
   if (owner == nullptr)
   {
      return;
   }

   std::lock_guard lock {g_handlerMutex};
   g_handlers.erase(owner);
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

   std::lock_guard lock {g_handlerMutex};
   for (const auto& [owner, handler] : g_handlers)
   {
      (void) owner;
      if (handler != nullptr)
      {
         handler(result, context);
      }
   }
}

} // namespace scwx::qt::render
