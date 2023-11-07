#include <scwx/qt/ui/setup/map_page.hpp>

namespace scwx
{
namespace qt
{
namespace ui
{
namespace setup
{

class MapPage::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;
};

MapPage::MapPage(QWidget* parent) :
    QWizardPage(parent), p {std::make_shared<Impl>()}
{
}

MapPage::~MapPage() = default;

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
