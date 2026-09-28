#pragma once

#include <QFrame>

#include <string>

namespace Ui
{
class PlacefileSettingsWidget;
}

namespace scwx
{
namespace qt
{
namespace ui
{

class PlacefileSettingsWidgetImpl;

class PlacefileSettingsWidget : public QFrame
{
   Q_OBJECT

public:
   // ShowAll: every placefile (the original, only behavior). ExcludeCategory/
   // OnlyCategory: filter by PlacefileModel's CategoryRole (see
   // manager::PlacefileManager::placefile_category) -- lets PlacefileDialog
   // show a "regular placefiles" tab and a "built-in Outlooks" tab as two
   // separate views over the same underlying PlacefileModel, rather than a
   // second model/manager.
   enum class CategoryMode
   {
      ShowAll,
      ExcludeCategory,
      OnlyCategory
   };

   explicit PlacefileSettingsWidget(QWidget*     parent = nullptr,
                                    CategoryMode mode   = CategoryMode::ShowAll,
                                    const std::string& category = {});
   ~PlacefileSettingsWidget();

private:
   friend class PlacefileSettingsWidgetImpl;
   std::unique_ptr<PlacefileSettingsWidgetImpl> p;
   Ui::PlacefileSettingsWidget*                 ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
