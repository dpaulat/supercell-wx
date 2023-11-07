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
   setTitle(tr("Map Configuration"));
   setSubTitle(tr("Configure the Supercell Wx map provider and basic layout."));
}

MapPage::~MapPage() = default;

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
