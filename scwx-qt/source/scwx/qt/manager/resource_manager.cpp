#include <scwx/qt/manager/resource_manager.hpp>
#include <scwx/qt/config/county_database.hpp>
#include <scwx/qt/model/imgui_context_model.hpp>
#include <scwx/qt/util/font.hpp>
#include <scwx/qt/util/texture_atlas.hpp>
#include <scwx/util/logger.hpp>

#include <execution>
#include <mutex>

#include <QFontDatabase>
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

static const std::unordered_map<types::Font, std::string> fontNames_ {
   {types::Font::din1451alt, ":/res/fonts/din1451alt.ttf"},
   {types::Font::din1451alt_g, ":/res/fonts/din1451alt_g.ttf"},
   {types::Font::Inconsolata_Regular, ":/res/fonts/Inconsolata-Regular.ttf"}};

static std::unordered_map<types::Font, int>                         fontIds_ {};
static std::unordered_map<types::Font, std::shared_ptr<util::Font>> fonts_ {};

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

std::shared_ptr<util::Font> Font(types::Font font)
{
   auto it = fonts_.find(font);
   if (it != fonts_.cend())
   {
      return it->second;
   }
   return nullptr;
}

bool LoadImageResource(const std::string& urlString)
{
   util::TextureAtlas& textureAtlas = util::TextureAtlas::Instance();
   return textureAtlas.CacheTexture(urlString, urlString);
}

void LoadImageResources(const std::vector<std::string>& urlStrings)
{
   std::mutex m {};
   bool       textureCached = false;

   std::for_each(std::execution::par_unseq,
                 urlStrings.begin(),
                 urlStrings.end(),
                 [&](auto& urlString)
                 {
                    bool value = LoadImageResource(urlString);

                    if (value)
                    {
                       std::unique_lock lock {m};
                       textureCached = true;
                    }
                 });

   if (textureCached)
   {
      util::TextureAtlas& textureAtlas = util::TextureAtlas::Instance();
      textureAtlas.BuildAtlas(1024, 1024);
   }
}

static void LoadFonts()
{
   for (auto& fontName : fontNames_)
   {
      int fontId = QFontDatabase::addApplicationFont(
         QString::fromStdString(fontName.second));
      fontIds_.emplace(fontName.first, fontId);

      auto font = util::Font::Create(fontName.second);
      fonts_.emplace(fontName.first, font);
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
   textureAtlas.BuildAtlas(1024, 1024);
}

} // namespace ResourceManager
} // namespace manager
} // namespace qt
} // namespace scwx
