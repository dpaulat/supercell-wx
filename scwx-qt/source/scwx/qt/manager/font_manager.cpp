#include <scwx/qt/manager/font_manager.hpp>
#include <scwx/util/logger.hpp>

#include <filesystem>
#include <fstream>

#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <boost/container_hash/hash.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
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

template<class Key>
struct FontRecordHash;

template<class Key>
struct FontRecordKeyEqual;

template<>
struct FontRecordHash<std::pair<FontRecord, int>>
{
   size_t operator()(const std::pair<FontRecord, int>& x) const;
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

   const std::vector<std::uint8_t>& GetRawFontData(const std::string& filename);

   static FontRecord MatchFontFile(const std::string&              family,
                                   const std::vector<std::string>& styles);

   std::string fontCachePath_ {};

   std::shared_mutex imguiFontAtlasMutex_ {};

   boost::unordered_flat_map<std::pair<FontRecord, int>,
                             std::shared_ptr<types::ImGuiFont>,
                             FontRecordHash<std::pair<FontRecord, int>>>
                     imguiFonts_ {};
   std::shared_mutex imguiFontsMutex_ {};

   boost::unordered_flat_map<std::string, std::vector<std::uint8_t>>
              rawFontData_ {};
   std::mutex rawFontDataMutex_ {};
};

FontManager::FontManager() : p(std::make_unique<Impl>()) {}

FontManager::~FontManager() {};

std::shared_mutex& FontManager::imgui_font_atlas_mutex()
{
   return p->imguiFontAtlasMutex_;
}

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

   // Only allow whole pixels, and clamp to 6-72 pt
   units::font_size::pixels<double> pixels {size};
   int imFontSize = std::clamp(static_cast<int>(pixels.value()), 8, 96);

   // Search for a loaded ImGui font
   {
      std::shared_lock imguiFontLock {p->imguiFontsMutex_};

      // Search for the associated ImGui font
      auto it = p->imguiFonts_.find(std::make_pair(fontRecord, imFontSize));
      if (it != p->imguiFonts_.end())
      {
         return it->second;
      }

      // No ImGui font was found, we need to create one
   }

   // Get raw font data
   const auto& rawFontData = p->GetRawFontData(fontRecord.filename_);

   // TODO: Create an ImGui font
   // TODO: imguiFontLock could be acquired during a render loop, when the font
   // atlas is already locked. Lock the font atlas first. Unless it's already
   // locked. It might need to be reentrant?
   // TODO: Search for font once more, to prevent loading the same font twice
   Q_UNUSED(rawFontData);

   return nullptr;
}

const std::vector<std::uint8_t>&
FontManager::Impl::GetRawFontData(const std::string& filename)
{
   std::unique_lock rawFontDataLock {rawFontDataMutex_};

   auto it = rawFontData_.find(filename);
   if (it != rawFontData_.end())
   {
      // Raw font data has already been loaded
      return it->second;
   }

   // Raw font data needs to be loaded
   std::basic_ifstream<std::uint8_t> ifs {filename, std::ios::binary};
   ifs.seekg(0, std::ios_base::end);
   std::size_t dataSize = ifs.tellg();
   ifs.seekg(0, std::ios_base::beg);

   // Store the font data in a buffer
   std::vector<std::uint8_t> buffer {};
   buffer.reserve(dataSize);
   std::copy(std::istreambuf_iterator<std::uint8_t>(ifs),
             std::istreambuf_iterator<std::uint8_t>(),
             std::back_inserter(buffer));

   // Place the buffer in the cache
   auto result = rawFontData_.emplace(filename, std::move(buffer));

   // Return the cached buffer
   return it->second;
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

size_t FontRecordHash<std::pair<FontRecord, int>>::operator()(
   const std::pair<FontRecord, int>& x) const
{
   size_t seed = 0;
   boost::hash_combine(seed, x.first.family_);
   boost::hash_combine(seed, x.first.style_);
   boost::hash_combine(seed, x.first.filename_);
   boost::hash_combine(seed, x.second);
   return seed;
}

bool operator==(const FontRecord& lhs, const FontRecord& rhs)
{
   return lhs.family_ == rhs.family_ && //
          lhs.style_ == rhs.style_ &&   //
          lhs.filename_ == rhs.filename_;
}

} // namespace manager
} // namespace qt
} // namespace scwx
