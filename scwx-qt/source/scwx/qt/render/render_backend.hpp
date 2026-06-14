#pragma once

#include <string_view>

namespace scwx::qt::render
{

enum class RenderBackend
{
   Vulkan
};

constexpr RenderBackend kRenderBackend = RenderBackend::Vulkan;

constexpr std::string_view RenderBackendName(RenderBackend backend)
{
   switch (backend)
   {
   case RenderBackend::Vulkan:
      return "Vulkan";
   }
   return "Unknown";
}

} // namespace scwx::qt::render
