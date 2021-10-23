#include <scwx/qt/manager/resource_manager.hpp>
#include <scwx/qt/util/font.hpp>

namespace scwx
{
namespace qt
{
namespace manager
{
namespace ResourceManager
{
static void LoadFonts();

void PreLoad()
{
   LoadFonts();
}

static void LoadFonts()
{
   util::Font::Create(":/res/fonts/din1451alt.ttf");
   util::Font::Create(":/res/fonts/din1451alt_g.ttf");
}

} // namespace ResourceManager
} // namespace manager
} // namespace qt
} // namespace scwx
