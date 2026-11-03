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

// QRhi normalized-device-coordinate convention for the compile-time backend.
// Mirrors QRhi::isYUpInNDC(): Y points up on Metal (like OpenGL / D3D), and
// down only on Vulkan. Every overlay shader writes the matrices below straight
// to gl_Position, so the matrix builders must pick the Y sign from this.
constexpr bool kNdcYUp = (kRenderBackend == RenderBackend::Metal);

// Sign applied to the Y scale of the map matrix so that north (+Y in the
// Mercator delta space) lands at the top of the framebuffer.
constexpr float kMapMatrixYSign = kNdcYUp ? 1.0f : -1.0f;

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
