#include <scwx/qt/ui/setup/finish_page.hpp>

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
};

FinishPage::FinishPage(QWidget* parent) :
    QWizardPage(parent), p {std::make_shared<Impl>()}
{
}

FinishPage::~FinishPage() = default;

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
