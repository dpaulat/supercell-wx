#pragma once

#include <QOpenGLWidget>

struct ImGuiContext;

namespace Ui
{
class ImGuiDebugWidget;
}

namespace scwx
{
namespace qt
{
namespace ui
{

class ImGuiDebugWidgetImpl;

class ImGuiDebugWidget : public QOpenGLWidget
{
private:
   Q_DISABLE_COPY(ImGuiDebugWidget)

public:
   explicit ImGuiDebugWidget(QWidget* parent = nullptr);
   ~ImGuiDebugWidget();

   std::string context_name() const;

   void set_current_context(ImGuiContext* context);

   void initializeGL() override;
   void paintGL() override;

private:
   friend class ImGuiDebugWidgetImpl;
   std::unique_ptr<ImGuiDebugWidgetImpl> p;
};

} // namespace ui
} // namespace qt
} // namespace scwx
