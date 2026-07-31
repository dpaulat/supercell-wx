#pragma once

#include <QWidget>

#include <memory>

namespace scwx::qt::map
{
class MapWidget;
}

namespace scwx::qt::ui
{

class ModelWidget : public QWidget
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(ModelWidget)

public:
   explicit ModelWidget(QWidget* parent = nullptr);
   ~ModelWidget() override;

   void SetMapWidget(map::MapWidget* mapWidget);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::ui
