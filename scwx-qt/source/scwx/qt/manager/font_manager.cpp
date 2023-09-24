#include <scwx/qt/manager/font_manager.hpp>
#include <scwx/util/logger.hpp>

#include <filesystem>

#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <fontconfig/fontconfig.h>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::font_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static const std::string kFcTrueType_ {"TrueType"};

struct FontRecord
{
   std::string family_ {};
   std::string style_ {};
   std::string filename_ {};
};

class FontManager::Impl
{
public:
   explicit Impl()
   {
      InitializeFontCache();
      InitializeFontconfig();
   }
   ~Impl() { FinalizeFontconfig(); }

   void FinalizeFontconfig();
   void InitializeFontCache();
   void InitializeFontconfig();

   static FontRecord MatchFontFile(const std::string&              family,
                                   const std::vector<std::string>& styles);

   std::string fontCachePath_ {};
};

FontManager::FontManager() : p(std::make_unique<Impl>()) {}

FontManager::~FontManager() {};

std::shared_ptr<types::ImGuiFont>
FontManager::GetImGuiFont(const std::string&               family,
                          const std::vector<std::string>&  styles,
                          units::font_size::points<double> size)
{
   const std::string styleString = fmt::format("{}", fmt::join(styles, " "));
   const std::string fontString =
      fmt::format("{}-{}:{}", family, size.value(), styleString);

   logger_->debug("LoadFontResource: {}", fontString);

   FontRecord fontRecord = Impl::MatchFontFile(family, styles);

   // TODO:
   Q_UNUSED(fontRecord);

   return nullptr;
}

void FontManager::LoadApplicationFont(const std::string& filename)
{
   // If the font cache failed to create, don't attempt to cache any fonts
   if (p->fontCachePath_.empty())
   {
      return;
   }

   // Make a copy of the font in the cache (if it doesn't exist)
   QFile     fontFile(QString::fromStdString(filename));
   QFileInfo fontFileInfo(fontFile);

   QFile       cacheFile(QString::fromStdString(p->fontCachePath_) +
                   fontFileInfo.fileName());
   QFileInfo   cacheFileInfo(cacheFile);
   std::string cacheFilename = cacheFile.fileName().toStdString();

   if (fontFile.exists())
   {
      // If the file has not been cached, or the font file size has changed
      if (!cacheFile.exists() || fontFileInfo.size() != cacheFileInfo.size())
      {
         logger_->info("Caching font: {}", filename);
         if (!fontFile.copy(cacheFile.fileName()))
         {
            logger_->error("Could not cache font: {}", filename);
            return;
         }
      }
   }
   else
   {
      logger_->error("Font does not exist: {}", filename);
      return;
   }

   // Load the file into fontconfig
   FcBool result = FcConfigAppFontAddFile(
      nullptr, reinterpret_cast<const FcChar8*>(cacheFilename.c_str()));
   if (!result)
   {
      logger_->error("Could not load font into fontconfig database", filename);
   }
}

void FontManager::Impl::InitializeFontCache()
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

void FontManager::Impl::InitializeFontconfig()
{
   FcConfig* fcConfig = FcInitLoadConfigAndFonts();
   FcConfigSetCurrent(fcConfig);
}

void FontManager::Impl::FinalizeFontconfig()
{
   FcFini();
}

FontRecord
FontManager::Impl::MatchFontFile(const std::string&              family,
                                 const std::vector<std::string>& styles)
{
   const std::string styleString = fmt::format("{}", fmt::join(styles, " "));
   const std::string fontString  = fmt::format("{}:{}", family, styleString);

   // Build fontconfig pattern
   FcPattern* pattern = FcPatternCreate();

   FcPatternAddString(
      pattern, FC_FAMILY, reinterpret_cast<const FcChar8*>(family.c_str()));
   FcPatternAddString(pattern,
                      FC_FONTFORMAT,
                      reinterpret_cast<const FcChar8*>(kFcTrueType_.c_str()));

   if (!styles.empty())
   {
      FcPatternAddString(pattern,
                         FC_STYLE,
                         reinterpret_cast<const FcChar8*>(styleString.c_str()));
   }

   // Perform font pattern match substitution
   FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
   FcDefaultSubstitute(pattern);

   // Find matching font
   FcResult   result;
   FcPattern* match = FcFontMatch(nullptr, pattern, &result);
   FontRecord record {};

   if (match != nullptr)
   {
      FcChar8* fcFamily;
      FcChar8* fcStyle;
      FcChar8* fcFile;

      // Match was found, get properties
      if (FcPatternGetString(match, FC_FAMILY, 0, &fcFamily) == FcResultMatch &&
          FcPatternGetString(match, FC_STYLE, 0, &fcStyle) == FcResultMatch &&
          FcPatternGetString(match, FC_FILE, 0, &fcFile) == FcResultMatch)
      {
         record.family_   = reinterpret_cast<char*>(fcFamily);
         record.style_    = reinterpret_cast<char*>(fcStyle);
         record.filename_ = reinterpret_cast<char*>(fcFile);

         logger_->debug("Found matching font: {}:{} ({})",
                        record.family_,
                        record.style_,
                        record.filename_);
      }
   }

   if (record.filename_.empty())
   {
      logger_->warn("Could not find matching font: {}", fontString);
   }

   // Cleanup
   FcPatternDestroy(match);
   FcPatternDestroy(pattern);

   return record;
}

FontManager& FontManager::Instance()
{
   static FontManager instance_ {};
   return instance_;
}

} // namespace manager
} // namespace qt
} // namespace scwx
