#include <scwx/qt/manager/font_manager.hpp>
#include <scwx/qt/map/draw_layer.hpp>
#include <scwx/qt/model/imgui_context_model.hpp>
#include <scwx/qt/gl/shader_program.hpp>
#include <scwx/util/logger.hpp>

#include <ranges>

#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_qt.hpp>
#include <utility>
#include <fmt/format.h>
#include <imgui.h>

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::draw_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class DrawLayer::Impl
{
public:
   explicit Impl(std::shared_ptr<gl::GlContext> glContext,
                 const std::string&             imGuiContextName) :
       glContext_ {std::move(glContext)}
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

      // Shutdown ImGui Context
      if (imGuiRendererInitialized_)
      {
         ImGui_ImplOpenGL3_Shutdown();
      }
      ImGui_ImplQt_Shutdown();

      // Destroy ImGui Context
      model::ImGuiContextModel::Instance().DestroyContext(imGuiContextName_);
   }

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   std::shared_ptr<gl::GlContext> glContext_;

   std::vector<std::shared_ptr<gl::draw::DrawItem>> drawList_ {};
   GLuint textureAtlas_ {GL_INVALID_INDEX};

   std::uint64_t textureAtlasBuildCount_ {};

   std::string   imGuiContextName_;
   ImGuiContext* imGuiContext_;
   bool          imGuiRendererInitialized_ {};
};

DrawLayer::DrawLayer(std::shared_ptr<gl::GlContext> glContext,
                     const std::string&             imGuiContextName) :
    GenericLayer(glContext),
    p(std::make_unique<Impl>(std::move(glContext), imGuiContextName))
{
}
DrawLayer::~DrawLayer() = default;

void DrawLayer::Initialize(const std::shared_ptr<MapContext>& mapContext)
{
   p->textureAtlas_ = p->glContext_->GetTextureAtlas();

   for (auto& item : p->drawList_)
   {
      item->Initialize();
   }

   ImGuiInitialize(mapContext);
}

void DrawLayer::ImGuiFrameStart(const std::shared_ptr<MapContext>& mapContext)
{
   auto defaultFont = manager::FontManager::Instance().GetImGuiFont(
      types::FontCategory::Default);

   // Setup ImGui Frame
   ImGui::SetCurrentContext(p->imGuiContext_);

   // Start ImGui Frame
   model::ImGuiContextModel::Instance().NewFrame();
   ImGui_ImplQt_NewFrame(mapContext->widget());
   ImGui_ImplOpenGL3_NewFrame();
   ImGui::NewFrame();
   ImGui::PushFont(defaultFont.first->font(), defaultFont.second.value());
}

void DrawLayer::ImGuiFrameEnd()
{
   // Pop default font
   ImGui::PopFont();

   // Render ImGui Frame
   ImGui::Render();
   ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void DrawLayer::ImGuiInitialize(const std::shared_ptr<MapContext>& mapContext)
{
   ImGui::SetCurrentContext(p->imGuiContext_);
   ImGui_ImplQt_RegisterWidget(mapContext->widget());
   ImGui_ImplOpenGL3_Init();
   p->imGuiRendererInitialized_ = true;
}

void DrawLayer::RenderWithoutImGui(
   const QMapLibre::CustomLayerRenderParameters& params)
{
   auto& glContext = p->glContext_;

   p->textureAtlas_ = glContext->GetTextureAtlas();

   // Determine if the texture atlas changed since last render
   const std::uint64_t newTextureAtlasBuildCount =
      glContext->texture_buffer_count();
   const bool textureAtlasChanged =
      newTextureAtlasBuildCount != p->textureAtlasBuildCount_;

   // Set OpenGL blend mode for transparency
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D_ARRAY, p->textureAtlas_);

   for (auto& item : p->drawList_)
   {
      item->Render(params, textureAtlasChanged);
   }

   p->textureAtlasBuildCount_ = newTextureAtlasBuildCount;
}

void DrawLayer::ImGuiSelectContext()
{
   ImGui::SetCurrentContext(p->imGuiContext_);
}

void DrawLayer::Render(const std::shared_ptr<MapContext>&            mapContext,
                       const QMapLibre::CustomLayerRenderParameters& params)
{
   ImGuiFrameStart(mapContext);
   RenderWithoutImGui(params);
   ImGuiFrameEnd();
}

void DrawLayer::Deinitialize()
{
   p->textureAtlas_ = GL_INVALID_INDEX;

   for (auto& item : p->drawList_)
   {
      item->Deinitialize();
   }
}

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

void DrawLayer::AddDrawItem(const std::shared_ptr<gl::draw::DrawItem>& drawItem)
{
   p->drawList_.push_back(drawItem);
}

} // namespace scwx::qt::map
