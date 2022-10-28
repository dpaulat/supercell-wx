#include <scwx/qt/manager/resource_manager.hpp>
#include <scwx/qt/config/county_database.hpp>
#include <scwx/qt/util/font.hpp>
#include <scwx/qt/util/texture_atlas.hpp>

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
   config::CountyDatabase::Initialize();

   util::Font::Create(":/res/fonts/din1451alt.ttf");
   util::Font::Create(":/res/fonts/din1451alt_g.ttf");

   util::TextureAtlas& textureAtlas = util::TextureAtlas::Instance();
   textureAtlas.RegisterTexture("lines/default-1x7",
                                ":/res/textures/lines/default-1x7.png");
   textureAtlas.RegisterTexture("lines/test-pattern",
                                ":/res/textures/lines/test-pattern.png");
   textureAtlas.BuildAtlas(8, 8);
}

} // namespace ResourceManager
} // namespace manager
} // namespace qt
} // namespace scwx
