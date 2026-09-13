#pragma once

#include <QWidget>

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

class ImGuiDebugWidget : public QWidget
{
private:
   Q_DISABLE_COPY(ImGuiDebugWidget)

public:
   explicit ImGuiDebugWidget(QWidget* parent = nullptr);
   ~ImGuiDebugWidget();

   std::string context_name() const;

   void set_current_context(ImGuiContext* context);

private:
   friend class ImGuiDebugWidgetImpl;
   std::unique_ptr<ImGuiDebugWidgetImpl> p;
};

} // namespace ui
} // namespace qt
} // namespace scwx
