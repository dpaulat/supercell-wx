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

class AudioCodecPage : public QWizardPage
{
   Q_DISABLE_COPY_MOVE(AudioCodecPage)

public:
   explicit AudioCodecPage(QWidget* parent = nullptr);
   ~AudioCodecPage();

   bool validatePage() override;

   static bool IsRequired();

private:
   class Impl;
   std::shared_ptr<Impl> p;
};

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
