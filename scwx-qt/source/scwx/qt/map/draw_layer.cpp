#include <scwx/qt/manager/font_manager.hpp>
#include <scwx/qt/map/draw_layer.hpp>
#include <scwx/qt/model/imgui_context_model.hpp>
#include <scwx/util/logger.hpp>

#include <ranges>

#include <backends/imgui_impl_qt.hpp>
#include <utility>
#include <fmt/format.h>
#include <imgui.h>

#include <scwx/qt/render/rhi_imgui_util.hpp>
#include <scwx/qt/render/rhi_vulkan_overlay.hpp>

#if !defined(__APPLE__)
#   include <backends/imgui_impl_vulkan.h>
#endif

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::draw_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class DrawLayer::Impl
{
public:
   explicit Impl(std::shared_ptr<render::RenderContext> renderContext,
                 const std::string&                     imGuiContextName) :
       renderContext_ {std::move(renderContext)}
   {
      static size_t currentLayerId_ {0u};
      imGuiContextName_ =
         fmt::format("{} {}", imGuiContextName, ++currentLayerId_);
      // This must be initialized after the last line
      // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
      imGuiContext_ =
         model::ImGuiContextModel::Instance().CreateContext(imGuiContextName_);

      // Initialize ImGui Qt backend
      ImGui_ImplQt_Init();
   }
   ~Impl()
   {
      // Set ImGui Context
      ImGui::SetCurrentContext(imGuiContext_);

      ImGui_ImplQt_Shutdown();

      // Destroy ImGui Context
      model::ImGuiContextModel::Instance().DestroyContext(imGuiContextName_);
   }

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   std::shared_ptr<render::RenderContext> renderContext_;

   std::vector<std::shared_ptr<draw::DrawItem>> drawList_ {};

   std::uint64_t textureAtlasBuildCount_ {};

   std::string   imGuiContextName_;
   ImGuiContext* imGuiContext_;
   bool          imGuiRendererInitialized_ {};
};

DrawLayer::DrawLayer(std::shared_ptr<render::RenderContext> renderContext,
                     const std::string&                     imGuiContextName) :
    GenericLayer(renderContext),
    p(std::make_unique<Impl>(std::move(renderContext), imGuiContextName))
{
}
DrawLayer::~DrawLayer() = default;

void DrawLayer::Initialize(const std::shared_ptr<MapContext>& mapContext)
{
   ImGuiInitialize(mapContext);
}

void DrawLayer::ImGuiFrameStart(const std::shared_ptr<MapContext>& mapContext)
{
   (void) mapContext;
}

void DrawLayer::ImGuiFrameEnd() {}

void DrawLayer::ImGuiInitialize(const std::shared_ptr<MapContext>& mapContext)
{
   ImGui::SetCurrentContext(p->imGuiContext_);
   ImGui_ImplQt_RegisterWidget(mapContext->widget());
   p->imGuiRendererInitialized_ = true;
}

void DrawLayer::RenderWithoutImGui(
   const QMapLibre::CustomLayerRenderParameters& params)
{
   (void) params;
}

void DrawLayer::ImGuiSelectContext()
{
   ImGui::SetCurrentContext(p->imGuiContext_);
}

void DrawLayer::ImGuiFrameStartVulkan(
   const std::shared_ptr<MapContext>& mapContext)
{
   auto defaultFont = manager::FontManager::Instance().GetImGuiFont(
      types::FontCategory::Default);

   ImGui::SetCurrentContext(p->imGuiContext_);

   model::ImGuiContextModel::Instance().NewFrame();
   ImGui_ImplQt_NewFrame(mapContext->widget());
#if !defined(__APPLE__)
   ImGui_ImplVulkan_NewFrame();
#endif
   ImGui::NewFrame();
   ImGui::PushFont(defaultFont.first->font(), defaultFont.second.value());
}

void DrawLayer::ImGuiFrameEndVulkan(QRhiCommandBuffer* commandBuffer)
{
   ImGui::PopFont();
   ImGui::Render();
   render::RenderImGuiDrawData(commandBuffer);
}

void DrawLayer::RenderWithoutImGuiVulkan(
   QRhiCommandBuffer*                            commandBuffer,
   render::RhiVulkanOverlayResources&            resources,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   const std::uint64_t newTextureAtlasBuildCount =
      p->renderContext_->texture_buffer_count();
   const bool textureAtlasChanged =
      newTextureAtlasBuildCount != p->textureAtlasBuildCount_;

   for (auto& item : p->drawList_)
   {
      item->RenderVulkan(commandBuffer, resources, params, textureAtlasChanged);
   }

   p->textureAtlasBuildCount_ = newTextureAtlasBuildCount;
}

void DrawLayer::RenderVulkanOverlay(
   QRhiCommandBuffer*                 commandBuffer,
   render::RhiVulkanOverlayResources& resources,
   const std::shared_ptr<MapContext>& /* mapContext */,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   RenderWithoutImGuiVulkan(commandBuffer, resources, params);
}

void DrawLayer::Render(const std::shared_ptr<MapContext>&            mapContext,
                       const QMapLibre::CustomLayerRenderParameters& params)
{
   ImGuiFrameStart(mapContext);
   RenderWithoutImGui(params);
   ImGuiFrameEnd();
}

void DrawLayer::Deinitialize() {}

bool DrawLayer::RunMousePicking(
   const std::shared_ptr<MapContext>& /* mapContext */,
   const QMapLibre::CustomLayerRenderParameters& params,
   const QPointF&                                mouseLocalPos,
   const QPointF&                                mouseGlobalPos,
   const glm::vec2&                              mouseCoords,
   const common::Coordinate&                     mouseGeoCoords,
   std::shared_ptr<types::EventHandler>&         eventHandler)
{
   bool itemPicked = false;

   // For each draw item in the draw list in reverse
   for (auto& it : std::ranges::reverse_view(p->drawList_))
   {
      // Run mouse picking on each draw item
      if (it->RunMousePicking(params,
                              mouseLocalPos,
                              mouseGlobalPos,
                              mouseCoords,
                              mouseGeoCoords,
                              eventHandler))
      {
         // If a draw item was picked, don't process additional items
         itemPicked = true;
         break;
      }
   }

   return itemPicked;
}

void DrawLayer::AddDrawItem(const std::shared_ptr<draw::DrawItem>& drawItem)
{
   p->drawList_.push_back(drawItem);
}

} // namespace scwx::qt::map
