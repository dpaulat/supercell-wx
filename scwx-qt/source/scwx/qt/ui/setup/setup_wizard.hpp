#pragma once

#include <QWizard>

namespace scwx
{
namespace qt
{
namespace ui
{
namespace setup
{

class SetupWizard : public QWizard
{
public:
   explicit SetupWizard(QWidget* parent = nullptr);
   ~SetupWizard();

private:
   class Impl;
   std::shared_ptr<Impl> p;
};

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
