#pragma once

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
};

[[nodiscard]] std::shared_ptr<RenderContext> CreateRenderContext();

} // namespace scwx::qt::render
