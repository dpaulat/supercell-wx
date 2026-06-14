#include <scwx/qt/vk/vk_context.hpp>
#include <scwx/qt/vk/vk_check.hpp>
#include <scwx/util/logger.hpp>

#include <QVersionNumber>
#include <QVulkanInstance>

#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace scwx::qt::vk
{

static const std::string logPrefix_ = "scwx::qt::vk::vk_context";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class VkContext::Impl
{
public:
   explicit Impl()  = default;
   ~Impl()          { Shutdown(); }

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void Initialize();
   void Shutdown();

   [[nodiscard]] bool IsInitialized() const { return initialized_; }

   QVulkanInstance  vulkanInstance_ {};
   VkPhysicalDevice physicalDevice_ {VK_NULL_HANDLE};
   VkDevice         device_ {VK_NULL_HANDLE};
   VkQueue          graphicsQueue_ {VK_NULL_HANDLE};
   uint32_t         graphicsQueueFamily_ {0};
   std::string      deviceName_ {};
   uint32_t         apiVersion_ {0};
   bool             initialized_ {false};
};

VkContext::VkContext() : p(std::make_unique<Impl>()) {}
VkContext::~VkContext() = default;

VkContext::VkContext(VkContext&&) noexcept            = default;
VkContext& VkContext::operator=(VkContext&&) noexcept = default;

void VkContext::Initialize()
{
   p->Initialize();
}

void VkContext::Shutdown()
{
   p->Shutdown();
}

bool VkContext::IsInitialized() const
{
   return p->IsInitialized();
}

VkPhysicalDevice VkContext::physical_device() const
{
   return p->physicalDevice_;
}

VkDevice VkContext::device() const
{
   return p->device_;
}

VkQueue VkContext::graphics_queue() const
{
   return p->graphicsQueue_;
}

uint32_t VkContext::graphics_queue_family() const
{
   return p->graphicsQueueFamily_;
}

std::string VkContext::device_name() const
{
   return p->deviceName_;
}

uint32_t VkContext::api_version() const
{
   return p->apiVersion_;
}

static bool IsDeviceSuitable(VkPhysicalDevice device, uint32_t& graphicsFamily)
{
   VkPhysicalDeviceProperties properties {};
   vkGetPhysicalDeviceProperties(device, &properties);

   if (properties.apiVersion < VK_API_VERSION_1_3)
   {
      return false;
   }

   uint32_t queueFamilyCount = 0;
   vkGetPhysicalDeviceQueueFamilyProperties(
      device, &queueFamilyCount, nullptr);

   std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
   vkGetPhysicalDeviceQueueFamilyProperties(
      device, &queueFamilyCount, queueFamilies.data());

   graphicsFamily = UINT32_MAX;
   for (uint32_t i = 0; i < queueFamilyCount; ++i)
   {
      if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u)
      {
         graphicsFamily = i;
         break;
      }
   }

   if (graphicsFamily == UINT32_MAX)
   {
      return false;
   }

   uint32_t extensionCount = 0;
   vkEnumerateDeviceExtensionProperties(
      device, nullptr, &extensionCount, nullptr);

   std::vector<VkExtensionProperties> extensions(extensionCount);
   vkEnumerateDeviceExtensionProperties(
      device, nullptr, &extensionCount, extensions.data());

   for (const auto& extension : extensions)
   {
      if (std::strcmp(extension.extensionName,
                      VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
      {
         return true;
      }
   }

   return false;
}

static int DeviceTypeScore(VkPhysicalDeviceType type)
{
   switch (type)
   {
   case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      return 3;
   case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      return 2;
   case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
      return 1;
   default:
      return 0;
   }
}

void VkContext::Impl::Initialize()
{
   if (initialized_)
   {
      return;
   }

   vulkanInstance_.setApiVersion(QVersionNumber(1, 3, 0));

   if (!vulkanInstance_.create())
   {
      logger_->error("QVulkanInstance::create failed");
      throw std::runtime_error("Unable to initialize Vulkan instance");
   }

   if (!vulkanInstance_.isValid())
   {
      logger_->error("QVulkanInstance is not valid");
      throw std::runtime_error("Invalid Vulkan instance");
   }

   if (vulkanInstance_.supportedApiVersion() < QVersionNumber(1, 3, 0))
   {
      logger_->error("Vulkan 1.3 is not supported by the Qt Vulkan instance");
      throw std::runtime_error("Vulkan 1.3 is required");
   }

   const VkInstance instance = vulkanInstance_.vkInstance();

   uint32_t deviceCount = 0;
   SCWX_VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));

   if (deviceCount == 0)
   {
      logger_->error("No Vulkan physical devices found");
      throw std::runtime_error("No Vulkan physical devices found");
   }

   std::vector<VkPhysicalDevice> devices(deviceCount);
   SCWX_VK_CHECK(
      vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));

   VkPhysicalDevice selectedDevice {VK_NULL_HANDLE};
   uint32_t         selectedQueueFamily {0};
   int              bestScore {-1};

   for (const VkPhysicalDevice device : devices)
   {
      uint32_t graphicsFamily = 0;
      if (!IsDeviceSuitable(device, graphicsFamily))
      {
         continue;
      }

      VkPhysicalDeviceProperties properties {};
      vkGetPhysicalDeviceProperties(device, &properties);

      const int score = DeviceTypeScore(properties.deviceType);
      if (score > bestScore)
      {
         bestScore           = score;
         selectedDevice      = device;
         selectedQueueFamily = graphicsFamily;
      }
   }

   if (selectedDevice == VK_NULL_HANDLE)
   {
      logger_->error(
         "No Vulkan 1.3-capable device with swapchain support was found");
      throw std::runtime_error("No suitable Vulkan 1.3 device found");
   }

   VkPhysicalDeviceProperties selectedProperties {};
   vkGetPhysicalDeviceProperties(selectedDevice, &selectedProperties);

   physicalDevice_        = selectedDevice;
   graphicsQueueFamily_   = selectedQueueFamily;
   deviceName_            = selectedProperties.deviceName;
   apiVersion_            = selectedProperties.apiVersion;

   const float queuePriority = 1.0f;

   VkDeviceQueueCreateInfo queueCreateInfo {};
   queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
   queueCreateInfo.queueFamilyIndex = graphicsQueueFamily_;
   queueCreateInfo.queueCount       = 1;
   queueCreateInfo.pQueuePriorities = &queuePriority;

   const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

   VkDeviceCreateInfo deviceCreateInfo {};
   deviceCreateInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   deviceCreateInfo.queueCreateInfoCount    = 1;
   deviceCreateInfo.pQueueCreateInfos         = &queueCreateInfo;
   deviceCreateInfo.enabledExtensionCount     = 1;
   deviceCreateInfo.ppEnabledExtensionNames   = deviceExtensions;

   SCWX_VK_CHECK(
      vkCreateDevice(physicalDevice_, &deviceCreateInfo, nullptr, &device_));

   vkGetDeviceQueue(
      device_, graphicsQueueFamily_, 0, &graphicsQueue_);

   logger_->info("Vulkan device: {}", deviceName_);
   logger_->info("Vulkan API version: {}.{}.{}",
                 VK_VERSION_MAJOR(apiVersion_),
                 VK_VERSION_MINOR(apiVersion_),
                 VK_VERSION_PATCH(apiVersion_));

   initialized_ = true;
}

void VkContext::Impl::Shutdown()
{
   if (device_ != VK_NULL_HANDLE)
   {
      vkDeviceWaitIdle(device_);
      vkDestroyDevice(device_, nullptr);
      device_ = VK_NULL_HANDLE;
   }

   if (vulkanInstance_.isValid())
   {
      vulkanInstance_.destroy();
   }

   physicalDevice_      = VK_NULL_HANDLE;
   graphicsQueue_       = VK_NULL_HANDLE;
   graphicsQueueFamily_ = 0;
   deviceName_.clear();
   apiVersion_          = 0;
   initialized_         = false;
}

} // namespace scwx::qt::vk
