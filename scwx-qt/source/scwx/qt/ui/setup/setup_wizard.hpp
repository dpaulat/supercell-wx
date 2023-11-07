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
   enum class Page : int
   {
      Welcome = 0,
      Map,
      Finish
   };

   explicit SetupWizard(QWidget* parent = nullptr);
   ~SetupWizard();

   static bool IsSetupRequired();

private:
   class Impl;
   std::shared_ptr<Impl> p;
};

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
