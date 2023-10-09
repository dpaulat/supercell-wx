#include <scwx/qt/manager/font_manager.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/settings/text_settings.hpp>
#include <scwx/util/logger.hpp>

#include <filesystem>
#include <fstream>

#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QStandardPaths>
#include <boost/container_hash/hash.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
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

typedef std::pair<FontRecord, units::font_size::pixels<int>> FontRecordPair;

template<class Key>
struct FontRecordHash;

template<>
struct FontRecordHash<FontRecordPair>
{
   size_t operator()(const FontRecordPair& x) const;
};

class FontManager::Impl
{
public:
   explicit Impl(FontManager* self) : self_ {self}
   {
      InitializeFontCache();
      InitializeFontconfig();
      ConnectSignals();
   }
   ~Impl() { FinalizeFontconfig(); }

   void ConnectSignals();
   void FinalizeFontconfig();
   void InitializeFontCache();
   void InitializeFontconfig();
   void UpdateImGuiFont(types::FontCategory fontCategory);

   const std::vector<char>& GetRawFontData(const std::string& filename);

   static FontRecord MatchFontFile(const std::string&              family,
                                   const std::vector<std::string>& styles);

   FontManager* self_;

   std::string fontCachePath_ {};

   std::shared_mutex imguiFontAtlasMutex_ {};

   std::uint64_t imguiFontsBuildCount_ {};

   boost::unordered_flat_map<FontRecordPair,
                             std::shared_ptr<types::ImGuiFont>,
                             FontRecordHash<FontRecordPair>>
                     imguiFonts_ {};
   std::shared_mutex imguiFontsMutex_ {};

   boost::unordered_flat_map<std::string, std::vector<char>> rawFontData_ {};
   std::mutex rawFontDataMutex_ {};

   std::shared_ptr<types::ImGuiFont> defaultFont_ {};
   boost::unordered_flat_map<types::FontCategory,
                             std::shared_ptr<types::ImGuiFont>>
              fontCategoryMap_ {};
   std::mutex fontCategoryMutex_ {};

   boost::unordered_flat_set<types::FontCategory> dirtyFonts_ {};
   std::mutex                                     dirtyFontsMutex_ {};

   boost::unordered_flat_map<types::Font, int> fontIds_ {};
};

FontManager::FontManager() : p(std::make_unique<Impl>(this)) {}

FontManager::~FontManager() {};

void FontManager::Impl::ConnectSignals()
{
   auto& textSettings = settings::TextSettings::Instance();

   for (auto fontCategory : types::FontCategoryIterator())
   {
      textSettings.font_family(fontCategory)
         .RegisterValueChangedCallback(
            [this, fontCategory](const auto&)
            {
               std::unique_lock lock {dirtyFontsMutex_};
               dirtyFonts_.insert(fontCategory);
            });
      textSettings.font_style(fontCategory)
         .RegisterValueChangedCallback(
            [this, fontCategory](const auto&)
            {
               std::unique_lock lock {dirtyFontsMutex_};
               dirtyFonts_.insert(fontCategory);
            });
      textSettings.font_point_size(fontCategory)
         .RegisterValueChangedCallback(
            [this, fontCategory](const auto&)
            {
               std::unique_lock lock {dirtyFontsMutex_};
               dirtyFonts_.insert(fontCategory);
            });
   }

   QObject::connect(
      &SettingsManager::Instance(),
      &SettingsManager::SettingsSaved,
      self_,
      [this]()
      {
         std::scoped_lock lock {dirtyFontsMutex_, fontCategoryMutex_};

         for (auto fontCategory : dirtyFonts_)
         {
            UpdateImGuiFont(fontCategory);
         }

         dirtyFonts_.clear();
      });
}

void FontManager::InitializeFonts()
{
   for (auto fontCategory : types::FontCategoryIterator())
   {
      p->UpdateImGuiFont(fontCategory);
   }
}

void FontManager::Impl::UpdateImGuiFont(types::FontCategory fontCategory)
{
   auto& textSettings = settings::TextSettings::Instance();

   auto family = textSettings.font_family(fontCategory).GetValue();
   auto styles = textSettings.font_style(fontCategory).GetValue();
   units::font_size::points<double> size {
      textSettings.font_point_size(fontCategory).GetValue()};

   fontCategoryMap_.insert_or_assign(
      fontCategory, self_->LoadImGuiFont(family, {styles}, size));
}

std::shared_mutex& FontManager::imgui_font_atlas_mutex()
{
   return p->imguiFontAtlasMutex_;
}

std::uint64_t FontManager::imgui_fonts_build_count() const
{
   return p->imguiFontsBuildCount_;
}

int FontManager::GetFontId(types::Font font) const
{
   auto it = p->fontIds_.find(font);
   if (it != p->fontIds_.cend())
   {
      return it->second;
   }
   return -1;
}

std::shared_ptr<types::ImGuiFont>
FontManager::GetImGuiFont(types::FontCategory fontCategory)
{
   std::unique_lock lock {p->fontCategoryMutex_};

   auto it = p->fontCategoryMap_.find(fontCategory);
   if (it != p->fontCategoryMap_.cend())
   {
      return it->second;
   }

   return p->defaultFont_;
}

QFont FontManager::GetQFont(types::FontCategory fontCategory)
{
   auto& textSettings = settings::TextSettings::Instance();

   auto family = textSettings.font_family(fontCategory).GetValue();
   auto styles = textSettings.font_style(fontCategory).GetValue();
   units::font_size::points<double> size {
      textSettings.font_point_size(fontCategory).GetValue()};

   QFont font(QString::fromStdString(family));
   font.setStyleName(QString::fromStdString(styles));
   font.setPointSizeF(size.value());

   return font;
}

std::shared_ptr<types::ImGuiFont>
FontManager::LoadImGuiFont(const std::string&               family,
                           const std::vector<std::string>&  styles,
                           units::font_size::points<double> size,
                           bool                             loadIfNotFound)
{
   const std::string styleString = fmt::format("{}", fmt::join(styles, " "));
   const std::string fontString =
      fmt::format("{}-{}:{}", family, size.value(), styleString);

   logger_->debug("LoadFontResource: {}", fontString);

   FontRecord fontRecord = Impl::MatchFontFile(family, styles);

   // Only allow whole pixels, and clamp to 6-72 pt
   units::font_size::pixels<double> pixels {size};
   units::font_size::pixels<int>    imFontSize {
      std::clamp(static_cast<int>(pixels.value()), 8, 96)};
   auto imguiFontKey = std::make_pair(fontRecord, imFontSize);

   // Search for a loaded ImGui font
   {
      std::shared_lock imguiFontLock {p->imguiFontsMutex_};

      // Search for the associated ImGui font
      auto it = p->imguiFonts_.find(imguiFontKey);
      if (it != p->imguiFonts_.end())
      {
         return it->second;
      }

      // No ImGui font was found, we need to create one
   }

   // No font was found, return an empty shared pointer if not loading
   if (!loadIfNotFound)
   {
      return nullptr;
   }

   // Get raw font data
   const auto& rawFontData = p->GetRawFontData(fontRecord.filename_);

   // The font atlas mutex might already be locked within an ImGui render frame.
   // Lock the font atlas mutex before the fonts mutex to prevent deadlock.
   std::unique_lock imguiFontAtlasLock {p->imguiFontAtlasMutex_};
   std::unique_lock imguiFontsLock {p->imguiFontsMutex_};

   // Search for the associated ImGui font again, to prevent loading the same
   // font twice
   auto it = p->imguiFonts_.find(imguiFontKey);
   if (it != p->imguiFonts_.end())
   {
      return it->second;
   }

   // Define a name for the ImGui font
   std::string fontName;
   try
   {
      fontName = fmt::format(
         "{}:{}",
         std::filesystem::path(fontRecord.filename_).filename().string(),
         imFontSize.value());
   }
   catch (const std::exception& ex)
   {
      logger_->warn(ex.what());
      fontName = fmt::format("{}:{}", fontRecord.filename_, imFontSize.value());
   }

   // Create an ImGui font
   std::shared_ptr<types::ImGuiFont> imguiFont =
      std::make_shared<types::ImGuiFont>(fontName, rawFontData, imFontSize);

   // Store the ImGui font
   p->imguiFonts_.insert_or_assign(imguiFontKey, imguiFont);

   // Increment ImGui font build count
   ++p->imguiFontsBuildCount_;

   // Return the ImGui font
   return imguiFont;
}

const std::vector<char>&
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
   std::basic_ifstream<char> ifs {filename, std::ios::binary};
   ifs.seekg(0, std::ios_base::end);
   std::size_t dataSize = ifs.tellg();
   ifs.seekg(0, std::ios_base::beg);

   // Store the font data in a buffer
   std::vector<char> buffer {};
   buffer.reserve(dataSize);
   std::copy(std::istreambuf_iterator<char>(ifs),
             std::istreambuf_iterator<char>(),
             std::back_inserter(buffer));

   // Place the buffer in the cache
   auto result = rawFontData_.emplace(filename, std::move(buffer));

   // Return the cached buffer
   return result.first->second;
}

void FontManager::LoadApplicationFont(types::Font        font,
                                      const std::string& filename)
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

   // Load the file into the Qt Font Database
   int fontId =
      QFontDatabase::addApplicationFont(QString::fromStdString(cacheFilename));
   p->fontIds_.emplace(font, fontId);

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

size_t FontRecordHash<FontRecordPair>::operator()(const FontRecordPair& x) const
{
   size_t seed = 0;
   boost::hash_combine(seed, x.first.family_);
   boost::hash_combine(seed, x.first.style_);
   boost::hash_combine(seed, x.first.filename_);
   boost::hash_combine(seed, x.second.value());
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
