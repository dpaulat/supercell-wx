#include <scwx/qt/ui/setup/setup_wizard.hpp>
#include <scwx/qt/ui/setup/finish_page.hpp>
#include <scwx/qt/ui/setup/map_page.hpp>
#include <scwx/qt/ui/setup/welcome_page.hpp>
#include <scwx/qt/settings/general_settings.hpp>

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
   setPage(static_cast<int>(Page::Map), new MapPage(this));
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

bool SetupWizard::IsSetupRequired()
{
   auto& generalSettings = settings::GeneralSettings::Instance();

   std::string mapboxApiKey   = generalSettings.mapbox_api_key().GetValue();
   std::string maptilerApiKey = generalSettings.maptiler_api_key().GetValue();

   // Setup is required if either API key is empty, or contains a single
   // character ("?")
   return (mapboxApiKey.size() <= 1 && maptilerApiKey.size() <= 1);
}

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
