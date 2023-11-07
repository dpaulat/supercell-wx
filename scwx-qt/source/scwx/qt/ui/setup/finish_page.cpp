#include <scwx/qt/ui/setup/finish_page.hpp>

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

class FinishPage::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;

   QVBoxLayout* layout_ {nullptr};
   QLabel*      finishLabel_ {nullptr};
};

FinishPage::FinishPage(QWidget* parent) :
    QWizardPage(parent), p {std::make_shared<Impl>()}
{
   setTitle(tr("Setup Complete"));
   setSubTitle(tr("Supercell Wx setup is complete!"));

   p->finishLabel_ =
      new QLabel(tr("Supercell Wx setup is now complete and ready for use. "
                    "Additional settings may be configured after startup. For "
                    "further information, please see the User Manual from the "
                    "Help menu, or join the Discord server for help."));
   p->finishLabel_->setWordWrap(true);

   p->layout_ = new QVBoxLayout(this);
   p->layout_->addWidget(p->finishLabel_);
   setLayout(p->layout_);
}

FinishPage::~FinishPage() = default;

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
