#include <scwx/qt/ui/setup/setup_wizard.hpp>
#include <scwx/qt/ui/setup/audio_codec_page.hpp>
#include <scwx/qt/ui/setup/finish_page.hpp>
#include <scwx/qt/ui/setup/map_layout_page.hpp>
#include <scwx/qt/ui/setup/map_provider_page.hpp>
#include <scwx/qt/ui/setup/welcome_page.hpp>

#include <QDesktopServices>
#include <QUrl>

namespace scwx
{
namespace qt
{
namespace ui
{
namespace setup
{

class SetupWizard::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;

   bool mapProviderPageIsRequired_ {MapProviderPage::IsRequired()};
   bool audioCodecPageIsRequired_ {AudioCodecPage::IsRequired()};
};

SetupWizard::SetupWizard(QWidget* parent) :
    QWizard(parent), p {std::make_shared<Impl>()}
{
   setWindowTitle(tr("Supercell Wx Setup"));
   setPixmap(QWizard::LogoPixmap, QPixmap(":/res/icons/scwx-64.png"));

   setOption(QWizard::WizardOption::IndependentPages);
   setOption(QWizard::WizardOption::NoBackButtonOnStartPage);
   setOption(QWizard::WizardOption::NoCancelButton);
   setOption(QWizard::WizardOption::HaveHelpButton);

   setPage(static_cast<int>(Page::Welcome), new WelcomePage(this));
   setPage(static_cast<int>(Page::MapProvider), new MapProviderPage(this));
   setPage(static_cast<int>(Page::MapLayout), new MapLayoutPage(this));
   setPage(static_cast<int>(Page::AudioCodec), new AudioCodecPage(this));
   setPage(static_cast<int>(Page::Finish), new FinishPage(this));

#if !defined(Q_OS_MAC)
   setWizardStyle(QWizard::WizardStyle::ModernStyle);
#endif

   connect(this,
           &QWizard::helpRequested,
           this,
           []() {
              QDesktopServices::openUrl(
                 QUrl {"https://supercell-wx.readthedocs.io/"});
           });
}

SetupWizard::~SetupWizard() = default;

int SetupWizard::nextId() const
{
   int nextId = currentId();

   while (true)
   {
      switch (++nextId)
      {
      case static_cast<int>(Page::MapProvider):
      case static_cast<int>(Page::MapLayout):
         if (p->mapProviderPageIsRequired_)
         {
            return nextId;
         }
         break;

      case static_cast<int>(Page::AudioCodec):
         if (p->audioCodecPageIsRequired_)
         {
            return nextId;
         }
         break;

      case static_cast<int>(Page::Finish):
         return nextId;

      default:
         return -1;
      }
   }

   return -1;
}

bool SetupWizard::IsSetupRequired()
{
   return (MapProviderPage::IsRequired() || AudioCodecPage::IsRequired());
}

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
