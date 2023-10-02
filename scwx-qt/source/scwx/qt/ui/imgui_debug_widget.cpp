#include <scwx/qt/ui/imgui_debug_widget.hpp>
#include <scwx/qt/manager/font_manager.hpp>
#include <scwx/qt/model/imgui_context_model.hpp>

#include <set>

#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_qt.hpp>
#include <fmt/format.h>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::imgui_debug_widget";

class ImGuiDebugWidgetImpl
{
public:
   explicit ImGuiDebugWidgetImpl(ImGuiDebugWidget* self) : self_ {self}
   {
      // Create ImGui Context
      static size_t currentIndex_ {0u};
      contextName_ = fmt::format("ImGui Debug {}", ++currentIndex_);
      context_ =
         model::ImGuiContextModel::Instance().CreateContext(contextName_);
      currentContext_ = context_;

      // Initialize ImGui Qt backend
      ImGui_ImplQt_Init();
      ImGui_ImplQt_RegisterWidget(self_);
   }

   ~ImGuiDebugWidgetImpl()
   {
      // Set ImGui Context
      ImGui::SetCurrentContext(context_);

      // Shutdown ImGui Context
      if (imGuiRendererInitialized_)
      {
         ImGui_ImplOpenGL3_Shutdown();
      }
      ImGui_ImplQt_Shutdown();

      // Destroy ImGui Context
      model::ImGuiContextModel::Instance().DestroyContext(contextName_);
   }

   ImGuiDebugWidget* self_;
   ImGuiContext*     context_;
   std::string       contextName_;

   ImGuiContext* currentContext_;

   std::set<ImGuiContext*> renderedSet_ {};
   bool                    imGuiRendererInitialized_ {false};
};

ImGuiDebugWidget::ImGuiDebugWidget(QWidget* parent) :
    QOpenGLWidget(parent), p {std::make_unique<ImGuiDebugWidgetImpl>(this)}
{
   // Accept focus for keyboard events
   setFocusPolicy(Qt::StrongFocus);
}

ImGuiDebugWidget::~ImGuiDebugWidget() {}

std::string ImGuiDebugWidget::context_name() const
{
   return p->contextName_;
}

void ImGuiDebugWidget::set_current_context(ImGuiContext* context)
{
   if (context == p->currentContext_)
   {
      return;
   }

   // Unregister widget with current context
   ImGui::SetCurrentContext(p->currentContext_);
   ImGui_ImplQt_UnregisterWidget(this);

   p->currentContext_ = context;

   // Register widget with new context
   ImGui::SetCurrentContext(context);
   ImGui_ImplQt_RegisterWidget(this);

   // Queue an update
   update();
}

void ImGuiDebugWidget::initializeGL()
{
   makeCurrent();

   // Initialize ImGui OpenGL3 backend
   ImGui::SetCurrentContext(p->context_);
   ImGui_ImplOpenGL3_Init();
   p->imGuiRendererInitialized_ = true;
}

void ImGuiDebugWidget::paintGL()
{
   ImGui::SetCurrentContext(p->currentContext_);

   // Lock ImGui font atlas prior to new ImGui frame
   std::shared_lock imguiFontAtlasLock {
      manager::FontManager::Instance().imgui_font_atlas_mutex()};

   ImGui_ImplQt_NewFrame(this);
   ImGui_ImplOpenGL3_NewFrame();

   ImGui::NewFrame();

   if (!p->renderedSet_.contains(p->currentContext_))
   {
      // Set initial position of demo window
      ImGui::SetNextWindowPos(ImVec2 {width() / 2.0f, height() / 2.0f},
                              ImGuiCond_FirstUseEver,
                              ImVec2 {0.5f, 0.5f});
      ImGui::Begin("Dear ImGui Demo");
      ImGui::End();

      p->renderedSet_.insert(p->currentContext_);
      update();
   }

   ImGui::ShowDemoWindow();

   ImGui::Render();
   ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

   // Unlock ImGui font atlas after rendering
   imguiFontAtlasLock.unlock();
}

} // namespace ui
} // namespace qt
} // namespace scwx
