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
   Q_DISABLE_COPY_MOVE(MapProviderPage)

public:
   explicit MapProviderPage(QWidget* parent = nullptr);
   ~MapProviderPage();

   bool isComplete() const override;
   bool validatePage() override;

private:
   class Impl;
   std::shared_ptr<Impl> p;
};

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
