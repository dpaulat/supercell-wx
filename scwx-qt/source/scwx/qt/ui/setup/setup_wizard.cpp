#include <scwx/qt/ui/setup/setup_wizard.hpp>

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
}

SetupWizard::~SetupWizard() = default;

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
