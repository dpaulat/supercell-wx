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

class MapPage : public QWizardPage
{
public:
   explicit MapPage(QWidget* parent = nullptr);
   ~MapPage();

private:
   class Impl;
   std::shared_ptr<Impl> p;
};

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
