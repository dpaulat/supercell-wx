#include <scwx/qt/ui/setup/welcome_page.hpp>

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
};

WelcomePage::WelcomePage(QWidget* parent) :
    QWizardPage(parent), p {std::make_shared<Impl>()}
{
}

WelcomePage::~WelcomePage() = default;

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
