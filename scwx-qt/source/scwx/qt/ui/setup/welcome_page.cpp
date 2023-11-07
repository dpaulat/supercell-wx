#include <scwx/qt/ui/setup/welcome_page.hpp>

#include <QLabel>
#include <QVBoxLayout>

namespace scwx
{
namespace qt
{
namespace ui
{
namespace setup
{

class WelcomePage::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;

   QVBoxLayout* layout_ {nullptr};
   QLabel*      welcomeLabel_ {nullptr};
};

WelcomePage::WelcomePage(QWidget* parent) :
    QWizardPage(parent), p {std::make_shared<Impl>()}
{
   setTitle(tr("Introduction"));
   setSubTitle(tr("Welcome to Supercell Wx!"));

   p->welcomeLabel_ =
      new QLabel(tr("Welcome to Supercell Wx. This wizard will guide you "
                    "through configuring Supercell Wx for initial use, as well "
                    "as introduce you to any new features."));
   p->welcomeLabel_->setWordWrap(true);

   p->layout_ = new QVBoxLayout(this);
   p->layout_->addWidget(p->welcomeLabel_);
   setLayout(p->layout_);
}

WelcomePage::~WelcomePage() = default;

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
