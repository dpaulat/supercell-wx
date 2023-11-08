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

class MapProviderPage : public QWizardPage
{
public:
   explicit MapProviderPage(QWidget* parent = nullptr);
   ~MapProviderPage();

private:
   class Impl;
   std::shared_ptr<Impl> p;
};

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
