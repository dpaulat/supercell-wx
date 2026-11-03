#include <scwx/qt/render/render_backend.hpp>
#include <scwx/qt/render/render_init.hpp>
#include <scwx/util/environment.hpp>
#include <scwx/util/logger.hpp>

#include <atomic>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <QCoreApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QMetaObject>
#if !defined(__APPLE__)
#   include <QVulkanFunctions>
#   include <QVulkanInstance>
#endif

namespace scwx::qt::render
{

static const std::string logPrefix_ = "scwx::qt::render::render_init";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr const char* kLegacyReleaseUrl =
   "https://github.com/dpaulat/supercell-wx/releases/tag/v0.6.2-release";

static std::atomic_bool graphicsWarningShown_ {false};
static std::string      graphicsProblem_ {};

/**
 * @brief Shows the unsupported graphics dialog.
 *
 * @return true if the user chose to continue anyway
 */
static bool ShowGraphicsWarning(const std::string& problem)
{
   QMessageBox box {
      QMessageBox::Warning,
      QObject::tr("Graphics Not Supported"),
      QObject::tr("Supercell Wx needs a graphics driver with "
                  "%1 support. No compatible device was found.")
         .arg(QString::fromUtf8(RenderBackendName(kRenderBackend).data()))};

   box.setInformativeText(
      QString::fromStdString(problem) + "\n\n" +
      QObject::tr("Try updating your graphics drivers. Version 0.6.2 "
                  "(OpenGL) remains available at <a href=\"%1\">%1</a> "
                  "if your hardware can not be supported.")
         .arg(kLegacyReleaseUrl));
   box.setTextFormat(Qt::RichText);
   box.setTextInteractionFlags(Qt::TextBrowserInteraction);

   auto* exitButton =
      box.addButton(QObject::tr("Exit"), QMessageBox::RejectRole);
   auto* continueButton =
      box.addButton(QObject::tr("Continue Anyway"), QMessageBox::AcceptRole);
   box.setDefaultButton(exitButton);
   box.setEscapeButton(exitButton);

   box.exec();
   return box.clickedButton() == continueButton;
}

#if !defined(__APPLE__)
/**
 * @brief Probes Vulkan with a throwaway instance.
 *
 * @return An empty string if a usable device exists, otherwise a description
 * of the problem
 */
static std::string ProbeVulkan()
{
   QVulkanInstance instance {};
   instance.setApiVersion(QVersionNumber(1, 0));

   if (!instance.create())
   {
      logger_->error("Vulkan instance creation failed (VkResult {})",
                     static_cast<int>(instance.errorCode()));
      return "No Vulkan driver was found.";
   }

   QVulkanFunctions* const f = instance.functions();

   std::uint32_t deviceCount = 0;
   f->vkEnumeratePhysicalDevices(instance.vkInstance(), &deviceCount, nullptr);
   std::vector<VkPhysicalDevice> devices(deviceCount);
   f->vkEnumeratePhysicalDevices(
      instance.vkInstance(), &deviceCount, devices.data());
   devices.resize(deviceCount);

   if (devices.empty())
   {
      logger_->error("No Vulkan physical devices found");
      return "No Vulkan-capable graphics device was found.";
   }

   std::string problem;
   for (VkPhysicalDevice device : devices)
   {
      VkPhysicalDeviceProperties properties {};
      f->vkGetPhysicalDeviceProperties(device, &properties);

      std::uint32_t extensionCount = 0;
      f->vkEnumerateDeviceExtensionProperties(
         device, nullptr, &extensionCount, nullptr);
      std::vector<VkExtensionProperties> extensions(extensionCount);
      f->vkEnumerateDeviceExtensionProperties(
         device, nullptr, &extensionCount, extensions.data());

      bool hasSwapchain = false;
      for (std::uint32_t i = 0; i < extensionCount; ++i)
      {
         hasSwapchain =
            hasSwapchain || std::string_view {extensions[i].extensionName} ==
                               VK_KHR_SWAPCHAIN_EXTENSION_NAME;
      }

      std::uint32_t queueCount = 0;
      f->vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, nullptr);
      std::vector<VkQueueFamilyProperties> queues(queueCount);
      f->vkGetPhysicalDeviceQueueFamilyProperties(
         device, &queueCount, queues.data());

      bool hasGraphicsQueue = false;
      for (const auto& queue : queues)
      {
         hasGraphicsQueue =
            hasGraphicsQueue || (queue.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
      }

      logger_->info(
         "Vulkan device: {} (API {}.{}.{}), swapchain: {}, "
         "graphics queue: {}",
         properties.deviceName,
         VK_VERSION_MAJOR(properties.apiVersion),
         VK_VERSION_MINOR(properties.apiVersion),
         VK_VERSION_PATCH(properties.apiVersion),
         hasSwapchain,
         hasGraphicsQueue);

      if (hasSwapchain && hasGraphicsQueue)
      {
         return {};
      }

      if (!problem.empty())
      {
         problem += "\n";
      }
      problem += fmt::format(
         "{} (Vulkan {}.{}) can not render to a window ({}).",
         properties.deviceName,
         VK_VERSION_MAJOR(properties.apiVersion),
         VK_VERSION_MINOR(properties.apiVersion),
         hasSwapchain ? "no graphics queue" : "missing VK_KHR_swapchain");
   }

   return problem;
}
#endif

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

bool CheckGraphicsSupport()
{
#if defined(__APPLE__)
   return true;
#else
   graphicsProblem_ = ProbeVulkan();
   if (graphicsProblem_.empty())
   {
      return true;
   }

   logger_->error("Vulkan is not supported: {}", graphicsProblem_);

   graphicsWarningShown_ = true;
   return ShowGraphicsWarning(graphicsProblem_);
#endif
}

void ReportGraphicsInitFailure(std::string_view reason)
{
   logger_->error("Graphics initialization failed: {}", reason);

   if (graphicsWarningShown_.exchange(true) ||
       QCoreApplication::instance() == nullptr)
   {
      return;
   }

   // Never run a modal event loop from inside a paint or initialize callback
   QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      []()
      {
         const std::string problem =
            graphicsProblem_.empty() ?
               std::string {
                  "The graphics device could not be initialized. "
                  "See the log file for details."} :
               graphicsProblem_;
         if (!ShowGraphicsWarning(problem))
         {
            QCoreApplication::exit(1);
         }
      },
      Qt::QueuedConnection);
}

} // namespace scwx::qt::render
