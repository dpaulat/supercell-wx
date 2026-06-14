#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <vulkan/vulkan.h>

namespace scwx::qt::vk
{

class VkContext
{
public:
   explicit VkContext();
   ~VkContext();

   VkContext(const VkContext&)            = delete;
   VkContext& operator=(const VkContext&) = delete;

   VkContext(VkContext&&) noexcept;
   VkContext& operator=(VkContext&&) noexcept;

   void Initialize();
   void Shutdown();

   [[nodiscard]] bool IsInitialized() const;

   [[nodiscard]] VkPhysicalDevice physical_device() const;
   [[nodiscard]] VkDevice         device() const;
   [[nodiscard]] VkQueue          graphics_queue() const;
   [[nodiscard]] uint32_t         graphics_queue_family() const;
   [[nodiscard]] std::string        device_name() const;
   [[nodiscard]] uint32_t           api_version() const;

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::vk
