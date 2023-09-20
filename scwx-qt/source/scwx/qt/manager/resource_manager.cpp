#include <scwx/qt/manager/resource_manager.hpp>
#include <scwx/qt/config/county_database.hpp>
#include <scwx/qt/model/imgui_context_model.hpp>
#include <scwx/qt/util/font.hpp>
#include <scwx/qt/util/texture_atlas.hpp>
#include <scwx/util/logger.hpp>

#include <execution>
#include <filesystem>
#include <mutex>

#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QStandardPaths>
#include <fontconfig/fontconfig.h>
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

static void InitializeFonts();
static void InitializeFontCache();
static void LoadFcApplicationFont(const std::string& fontFilename);
static void LoadFonts();
static void LoadTextures();

static const std::unordered_map<types::Font, std::string> fontNames_ {
   {types::Font::din1451alt, ":/res/fonts/din1451alt.ttf"},
   {types::Font::din1451alt_g, ":/res/fonts/din1451alt_g.ttf"},
   {types::Font::Inconsolata_Regular, ":/res/fonts/Inconsolata-Regular.ttf"}};

static std::string fontCachePath_ {};

static std::unordered_map<types::Font, int>                         fontIds_ {};
static std::unordered_map<types::Font, std::shared_ptr<util::Font>> fonts_ {};

void Initialize()
{
   config::CountyDatabase::Initialize();

   InitializeFonts();

   LoadFonts();
   LoadTextures();
}

void Shutdown()
{
   // Finalize Fontconfig
   FcFini();
}

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

std::shared_ptr<boost::gil::rgba8_image_t>
LoadImageResource(const std::string& urlString)
{
   util::TextureAtlas& textureAtlas = util::TextureAtlas::Instance();
   return textureAtlas.CacheTexture(urlString, urlString);
}

std::vector<std::shared_ptr<boost::gil::rgba8_image_t>>
LoadImageResources(const std::vector<std::string>& urlStrings)
{
   std::mutex                                              m {};
   std::vector<std::shared_ptr<boost::gil::rgba8_image_t>> images {};

   std::for_each(std::execution::par_unseq,
                 urlStrings.begin(),
                 urlStrings.end(),
                 [&](auto& urlString)
                 {
                    auto image = LoadImageResource(urlString);

                    if (image != nullptr)
                    {
                       std::unique_lock lock {m};
                       images.emplace_back(std::move(image));
                    }
                 });

   if (!images.empty())
   {
      util::TextureAtlas& textureAtlas = util::TextureAtlas::Instance();
      textureAtlas.BuildAtlas(2048, 2048);
   }

   return images;
}

static void InitializeFonts()
{
   // Create a directory to store local fonts
   InitializeFontCache();

   // Initialize Fontconfig
   FcConfig* fcConfig = FcInitLoadConfigAndFonts();
   FcConfigSetCurrent(fcConfig);
}

static void InitializeFontCache()
{
   std::string cachePath {
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
         .toStdString() +
      "/fonts"};

   fontCachePath_ = cachePath + "/";

   if (!std::filesystem::exists(cachePath))
   {
      std::error_code error;
      if (!std::filesystem::create_directories(cachePath, error))
      {
         logger_->error("Unable to create font cache directory: \"{}\" ({})",
                        cachePath,
                        error.message());
         fontCachePath_.clear();
      }
   }
}

static void LoadFonts()
{
   for (auto& fontName : fontNames_)
   {
      int fontId = QFontDatabase::addApplicationFont(
         QString::fromStdString(fontName.second));
      fontIds_.emplace(fontName.first, fontId);

      LoadFcApplicationFont(fontName.second);

      auto font = util::Font::Create(fontName.second);
      fonts_.emplace(fontName.first, font);
   }

   ImFontAtlas* fontAtlas = model::ImGuiContextModel::Instance().font_atlas();
   fontAtlas->AddFontDefault();
}

static void LoadFcApplicationFont(const std::string& fontFilename)
{
   // If the font cache failed to create, don't attempt to cache any fonts
   if (fontCachePath_.empty())
   {
      return;
   }

   // Make a copy of the font in the cache (if it doesn't exist)
   QFile     fontFile(QString::fromStdString(fontFilename));
   QFileInfo fontFileInfo(fontFile);

   QFile       cacheFile(QString::fromStdString(fontCachePath_) +
                   fontFileInfo.fileName());
   QFileInfo   cacheFileInfo(cacheFile);
   std::string cacheFilename = cacheFile.fileName().toStdString();

   if (fontFile.exists())
   {
      // If the file has not been cached, or the font file size has changed
      if (!cacheFile.exists() || fontFileInfo.size() != cacheFileInfo.size())
      {
         logger_->info("Caching font: {}", fontFilename);
         if (!fontFile.copy(cacheFile.fileName()))
         {
            logger_->error("Could not cache font: {}", fontFilename);
            return;
         }
      }
   }
   else
   {
      logger_->error("Font does not exist: {}", fontFilename);
      return;
   }

   // Load the file into fontconfig (FcConfigAppFontAddFile)
   FcBool result = FcConfigAppFontAddFile(
      nullptr, reinterpret_cast<const FcChar8*>(cacheFilename.c_str()));
   if (!result)
   {
      logger_->error("Could not load font into fontconfig database",
                     fontFilename);
   }
}

static void LoadTextures()
{
   util::TextureAtlas& textureAtlas = util::TextureAtlas::Instance();
   textureAtlas.RegisterTexture("lines/default-1x7",
                                ":/res/textures/lines/default-1x7.png");
   textureAtlas.RegisterTexture("lines/test-pattern",
                                ":/res/textures/lines/test-pattern.png");
   textureAtlas.BuildAtlas(2048, 2048);
}

} // namespace ResourceManager
} // namespace manager
} // namespace qt
} // namespace scwx
