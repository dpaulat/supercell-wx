#include <scwx/qt/ui/setup/map_layout_page.hpp>

namespace scwx
{
namespace qt
{
namespace ui
{
namespace setup
{

class MapLayoutPage::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;
};

MapLayoutPage::MapLayoutPage(QWidget* parent) :
    QWizardPage(parent), p {std::make_shared<Impl>()}
{
   setTitle(tr("Map Layout"));
   setSubTitle(tr("Configure the Supercell Wx map layout."));
}

MapLayoutPage::~MapLayoutPage() = default;

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
