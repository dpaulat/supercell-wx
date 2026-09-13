#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>

namespace scwx::qt::render
{

class RenderContext
{
public:
   virtual ~RenderContext() = default;

   virtual void Initialize() = 0;
   virtual void StartFrame() = 0;

   [[nodiscard]] virtual std::uint64_t texture_buffer_count() const = 0;

   void set_layer_opacity(float opacity)
   {
      layerOpacity_ = std::clamp(opacity, 0.0f, 1.0f);
   }

   [[nodiscard]] float layer_opacity() const { return layerOpacity_; }

private:
   float layerOpacity_ {1.0f};
};

[[nodiscard]] std::shared_ptr<RenderContext> CreateRenderContext();

} // namespace scwx::qt::render
