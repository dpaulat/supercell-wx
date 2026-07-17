#pragma once

#include <string_view>

namespace scwx::qt::render
{

enum class RenderBackend
{
   Vulkan,
   Metal
};

#if defined(__APPLE__)
constexpr RenderBackend kRenderBackend = RenderBackend::Metal;
#else
constexpr RenderBackend kRenderBackend = RenderBackend::Vulkan;
#endif

constexpr std::string_view RenderBackendName(RenderBackend backend)
{
   switch (backend)
   {
   case RenderBackend::Vulkan:
      return "Vulkan";
   case RenderBackend::Metal:
      return "Metal";
   }
   return "Unknown";
}

} // namespace scwx::qt::render
