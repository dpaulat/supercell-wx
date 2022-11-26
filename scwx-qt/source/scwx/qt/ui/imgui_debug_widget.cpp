#include <scwx/qt/ui/imgui_debug_widget.hpp>

#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_qt.hpp>

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
      // ImGui Configuration
      auto& io = ImGui::GetIO();

      // Initialize Qt backend
      ImGui_ImplQt_RegisterWidget(self_);
   }
   ~ImGuiDebugWidgetImpl() {}

   ImGuiDebugWidget* self_;
};

ImGuiDebugWidget::ImGuiDebugWidget(QWidget* parent) :
    QOpenGLWidget(parent), p {std::make_unique<ImGuiDebugWidgetImpl>(this)}
{
}

void ImGuiDebugWidget::initializeGL() {}

void ImGuiDebugWidget::paintGL()
{
   ImGui_ImplQt_NewFrame(this);
   ImGui_ImplOpenGL3_NewFrame();

   ImGui::NewFrame();

   ImGui::ShowDemoWindow();

   ImGui::Render();
   ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

ImGuiDebugWidget::~ImGuiDebugWidget() {}

} // namespace ui
} // namespace qt
} // namespace scwx
