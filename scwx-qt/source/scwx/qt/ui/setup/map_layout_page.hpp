#pragma once

#include <QWizardPage>

namespace scwx
{
namespace qt
{
namespace ui
{
namespace setup
{

class MapLayoutPage : public QWizardPage
{
public:
   explicit MapLayoutPage(QWidget* parent = nullptr);
   ~MapLayoutPage();

   bool validatePage() override;

private:
   class Impl;
   std::shared_ptr<Impl> p;
};

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
