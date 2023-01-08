#include <scwx/qt/manager/resource_manager.hpp>
#include <scwx/qt/config/county_database.hpp>
#include <scwx/qt/model/imgui_context_model.hpp>
#include <scwx/qt/util/font.hpp>
#include <scwx/qt/util/texture_atlas.hpp>
#include <scwx/util/logger.hpp>

#include <imgui.h>

#include <QFontDatabase>

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

static const std::unordered_map<types::Font, std::string> fontNames_ {
   {types::Font::din1451alt, ":/res/fonts/din1451alt.ttf"},
   {types::Font::din1451alt_g, ":/res/fonts/din1451alt_g.ttf"}};

static std::unordered_map<types::Font, int> fontIds_ {};

void Initialize()
{
   config::CountyDatabase::Initialize();

   LoadFonts();
   LoadTextures();
}

void Shutdown() {}

int FontId(types::Font font)
{
   auto it = fontIds_.find(font);
   if (it != fontIds_.cend())
   {
      return it->second;
   }
   return -1;
}

static void LoadFonts()
{
   for (auto& fontName : fontNames_)
   {
      int fontId = QFontDatabase::addApplicationFont(
         QString::fromStdString(fontName.second));
      fontIds_.emplace(fontName.first, fontId);

      util::Font::Create(fontName.second);
   }

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
