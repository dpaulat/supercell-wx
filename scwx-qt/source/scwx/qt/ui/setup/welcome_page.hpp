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

class WelcomePage : public QWizardPage
{
public:
   explicit WelcomePage(QWidget* parent = nullptr);
   ~WelcomePage();

private:
   class Impl;
   std::shared_ptr<Impl> p;
};

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
