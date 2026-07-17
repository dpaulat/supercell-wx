#include <scwx/qt/render/render_backend.hpp>
#include <scwx/qt/render/render_init.hpp>
#include <scwx/util/environment.hpp>
#include <scwx/util/logger.hpp>

#include <QCoreApplication>

namespace scwx::qt::render
{

static const std::string logPrefix_ = "scwx::qt::render::render_init";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

void InitializeGraphics()
{
   logger_->info("Render backend: {}", RenderBackendName(kRenderBackend));

   if constexpr (kRenderBackend != RenderBackend::Vulkan)
   {
      return;
   }

   const bool validationRequested =
      scwx::util::HasEnvironment("SCWX_VULKAN_VALIDATION");
   const bool validationDisabled =
      scwx::util::HasEnvironment("SCWX_VULKAN_VALIDATION_DISABLE");

#if !defined(NDEBUG)
   const bool enableValidation = validationRequested || !validationDisabled;
#else
   const bool enableValidation = validationRequested && !validationDisabled;
#endif

   if (enableValidation)
   {
      qputenv("VK_INSTANCE_LAYERS", "VK_LAYER_KHRONOS_validation");
      logger_->info(
         "Vulkan validation layers enabled (SCWX_VULKAN_VALIDATION)");
   }
}

} // namespace scwx::qt::render
