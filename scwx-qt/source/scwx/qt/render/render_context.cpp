#include <scwx/qt/render/render_context.hpp>
#include <scwx/qt/util/texture_atlas.hpp>

namespace scwx::qt::render
{

class VulkanRenderContext : public RenderContext
{
public:
   void Initialize() override {}
   void StartFrame() override {}

   [[nodiscard]] std::uint64_t texture_buffer_count() const override
   {
      return util::TextureAtlas::Instance().BuildCount();
   }
};

std::shared_ptr<RenderContext> CreateRenderContext()
{
   return std::make_shared<VulkanRenderContext>();
}

} // namespace scwx::qt::render
