#include <scwx/qt/manager/resource_manager.hpp>
#include <scwx/qt/config/county_database.hpp>
#include <scwx/qt/model/imgui_context_model.hpp>
#include <scwx/qt/util/font.hpp>
#include <scwx/qt/util/texture_atlas.hpp>
#include <scwx/util/logger.hpp>

#include <imgui.h>

namespace scwx
{
namespace qt
{
namespace manager
{
namespace ResourceManager
{
static const std::string logPrefix_ = "scwx::qt::manager::resource_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static void LoadFonts();
static void LoadTextures();

void Initialize()
{
   config::CountyDatabase::Initialize();

   LoadFonts();
   LoadTextures();
}

void Shutdown() {}

static void LoadFonts()
{
   util::Font::Create(":/res/fonts/din1451alt.ttf");
   util::Font::Create(":/res/fonts/din1451alt_g.ttf");

   ImFontAtlas* fontAtlas = model::ImGuiContextModel::Instance().font_atlas();
   fontAtlas->AddFontDefault();
}

static void LoadTextures()
{
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
