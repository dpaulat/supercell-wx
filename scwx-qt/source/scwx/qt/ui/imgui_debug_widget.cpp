#include <scwx/qt/ui/imgui_debug_widget.hpp>
#include <scwx/qt/model/imgui_context_model.hpp>

#include <set>

#include <imgui.h>
#include <backends/imgui_impl_qt.hpp>
#include <fmt/format.h>

namespace scwx::qt::ui
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

      ImGui_ImplQt_Shutdown();

      // Destroy ImGui Context
      model::ImGuiContextModel::Instance().DestroyContext(contextName_);
   }

   ImGuiDebugWidget* self_;
   ImGuiContext*     context_;
   std::string       contextName_;

   ImGuiContext* currentContext_;

   std::set<ImGuiContext*> renderedSet_ {};
};

ImGuiDebugWidget::ImGuiDebugWidget(QWidget* parent) :
    QWidget(parent), p {std::make_unique<ImGuiDebugWidgetImpl>(this)}
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

} // namespace scwx::qt::ui
