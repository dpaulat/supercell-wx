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

class FinishPage : public QWizardPage
{
public:
   explicit FinishPage(QWidget* parent = nullptr);
   ~FinishPage();

private:
   class Impl;
   std::shared_ptr<Impl> p;
};

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
