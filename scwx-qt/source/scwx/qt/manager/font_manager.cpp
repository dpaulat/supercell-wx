#include <scwx/qt/manager/font_manager.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/main/application_paths.hpp>
#include <scwx/qt/model/imgui_context_model.hpp>
#include <scwx/qt/settings/text_settings.hpp>
#include <scwx/util/environment.hpp>
#include <scwx/util/logger.hpp>

#include <filesystem>
#include <fstream>

#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <boost/container_hash/hash.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <fmt/ranges.h>
#include <fontconfig/fontconfig.h>
#include <imgui.h>

namespace scwx::qt::manager
{

static const std::string logPrefix_ = "scwx::qt::manager::font_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static const std::string kFcTrueType_ {"TrueType"};
static const std::string kFcOpenType_ {"CFF"};

struct FontRecord
{
   std::string family_ {};
   std::string style_ {};
   std::string filename_ {};
};

template<class Key>
struct FontRecordHash;

template<>
struct FontRecordHash<FontRecord>
{
   size_t operator()(const FontRecord& x) const;
};

class FontManager::Impl
{
public:
   explicit Impl(FontManager* self) : self_ {self}
   {
      InitializeEnvironment();
      InitializeFontCache();
      InitializeFontconfig();
      ConnectSignals();
   }
   ~Impl() { FinalizeFontconfig(); }

   void ConnectSignals();
   void FinalizeFontconfig();
   void InitializeEnvironment();
   void InitializeFontCache();
   void InitializeFontconfig();
   void UpdateImGuiFont(types::FontCategory fontCategory);
   void UpdateQFont(types::FontCategory fontCategory);
   void UpdateImGuiFontForCurrentAtlas(types::FontCategory fontCategory);

   static ImFontAtlas* CurrentAtlas();

   const std::vector<char>& GetRawFontData(const std::string& filename);

   static bool       CheckFontFormat(const FcChar8* format);
   static FontRecord MatchFontFile(const std::string&              family,
                                   const std::vector<std::string>& styles);

   FontManager* self_;

   std::string fontCachePath_ {};

   std::shared_mutex imguiFontAtlasMutex_ {};

   // ImGui fonts are cached per font atlas. On Vulkan, each map pane uses its
   // own ImFontAtlas (so descriptor sets do not collide across per-pane Vulkan
   // backends); each backend owns the atlas for its renderer.
   boost::unordered_flat_map<
      ImFontAtlas*,
      boost::unordered_flat_map<FontRecord,
                                std::shared_ptr<types::ImGuiFont>,
                                FontRecordHash<FontRecord>>>
                     imguiFontsByAtlas_ {};
   std::shared_mutex imguiFontsMutex_ {};

   boost::unordered_flat_map<std::string, std::vector<char>> rawFontData_ {};
   std::mutex rawFontDataMutex_ {};

   std::pair<std::shared_ptr<types::ImGuiFont>, units::font_size::pixels<float>>
      defaultFont_ {};
   boost::unordered_flat_map<
      ImFontAtlas*,
      boost::unordered_flat_map<types::FontCategory,
                                std::pair<std::shared_ptr<types::ImGuiFont>,
                                          units::font_size::pixels<float>>>>
      fontCategoryImguiFontByAtlas_ {};
   boost::unordered_flat_map<types::FontCategory, QFont>
              fontCategoryQFontMap_ {};
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

   QObject::connect(&SettingsManager::Instance(),
                    &SettingsManager::SettingsSaved,
                    self_,
                    [this]()
                    {
                       const std::scoped_lock lock {dirtyFontsMutex_,
                                                    fontCategoryMutex_};

                       for (auto fontCategory : dirtyFonts_)
                       {
                          UpdateImGuiFont(fontCategory);
                          UpdateQFont(fontCategory);
                       }

                       dirtyFonts_.clear();
                    });
}

void FontManager::InitializeFonts()
{
   for (auto fontCategory : types::FontCategoryIterator())
   {
      p->UpdateImGuiFont(fontCategory);
      p->UpdateQFont(fontCategory);
   }
}

void FontManager::EnsureImGuiFontsBuilt()
{
   if (ImGui::GetCurrentContext() == nullptr || ImGui::GetIO().Fonts == nullptr)
   {
      return;
   }

   // Build each category for the current atlas if not already cached. Must run
   // before the caller takes a shared lock on the atlas mutex (GetImGuiFont may
   // acquire it exclusively to create fonts).
   for (auto fontCategory : types::FontCategoryIterator())
   {
      (void) GetImGuiFont(fontCategory);
   }
}

units::font_size::pixels<float>
FontManager::ImFontSize(units::font_size::pixels<double> size)
{
   static constexpr units::font_size::pixels<int> kMinFontSize_ {8};
   static constexpr units::font_size::pixels<int> kMaxFontSize_ {96};

   // Only allow whole pixels, and clamp to 6-72 pt
   const units::font_size::pixels<double> pixels {size};
   const units::font_size::pixels<int>    imFontSize {
      std::clamp(static_cast<int>(pixels.value()),
                 kMinFontSize_.value(),
                 kMaxFontSize_.value())};

   return imFontSize;
}

ImFontAtlas* FontManager::Impl::CurrentAtlas()
{
   if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().Fonts != nullptr)
   {
      return ImGui::GetIO().Fonts;
   }

   return model::ImGuiContextModel::Instance().font_atlas();
}

void FontManager::Impl::UpdateImGuiFont(types::FontCategory fontCategory)
{
   UpdateImGuiFontForCurrentAtlas(fontCategory);
}

void FontManager::Impl::UpdateImGuiFontForCurrentAtlas(
   types::FontCategory fontCategory)
{
   auto& textSettings = settings::TextSettings::Instance();

   auto family = textSettings.font_family(fontCategory).GetValue();
   auto styles = textSettings.font_style(fontCategory).GetValue();
   units::font_size::points<double> size {
      textSettings.font_point_size(fontCategory).GetValue()};

   ImFontAtlas* atlas = CurrentAtlas();

   fontCategoryImguiFontByAtlas_[atlas].insert_or_assign(
      fontCategory,
      std::make_pair(self_->LoadImGuiFont(family, {styles}, true),
                     ImFontSize(size)));
}

void FontManager::Impl::UpdateQFont(types::FontCategory fontCategory)
{
   auto& textSettings = settings::TextSettings::Instance();

   auto family = textSettings.font_family(fontCategory).GetValue();
   auto styles = textSettings.font_style(fontCategory).GetValue();
   units::font_size::points<double> size {
      textSettings.font_point_size(fontCategory).GetValue()};

   QFont font = QFontDatabase::font(QString::fromStdString(family),
                                    QString::fromStdString(styles),
                                    static_cast<int>(size.value()));
   font.setHintingPreference(QFont::HintingPreference::PreferNoHinting);

#if !defined(__APPLE__)
   font.setPointSizeF(size.value());
#else
   const units::font_size::pixels<double> pixelSize {size};
   font.setPixelSize(static_cast<int>(pixelSize.value()));
#endif

   fontCategoryQFontMap_.insert_or_assign(fontCategory, font);
}

std::shared_mutex& FontManager::imgui_font_atlas_mutex()
{
   return p->imguiFontAtlasMutex_;
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

std::pair<std::shared_ptr<types::ImGuiFont>, units::font_size::pixels<float>>
FontManager::GetImGuiFont(types::FontCategory fontCategory)
{
   std::unique_lock lock {p->fontCategoryMutex_};

   ImFontAtlas* atlas = Impl::CurrentAtlas();

   auto atlasIt = p->fontCategoryImguiFontByAtlas_.find(atlas);
   if (atlasIt != p->fontCategoryImguiFontByAtlas_.cend())
   {
      auto it = atlasIt->second.find(fontCategory);
      if (it != atlasIt->second.cend())
      {
         return it->second;
      }
   }

   // Not yet built for this atlas; build it now (current context is set)
   p->UpdateImGuiFontForCurrentAtlas(fontCategory);

   atlasIt = p->fontCategoryImguiFontByAtlas_.find(atlas);
   if (atlasIt != p->fontCategoryImguiFontByAtlas_.cend())
   {
      auto it = atlasIt->second.find(fontCategory);
      if (it != atlasIt->second.cend())
      {
         return it->second;
      }
   }

   return p->defaultFont_;
}

QFont FontManager::GetQFont(types::FontCategory fontCategory)
{
   std::unique_lock lock {p->fontCategoryMutex_};

   auto it = p->fontCategoryQFontMap_.find(fontCategory);
   if (it != p->fontCategoryQFontMap_.cend())
   {
      return it->second;
   }

   return QGuiApplication::font();
}

std::shared_ptr<types::ImGuiFont>
FontManager::LoadImGuiFont(const std::string&              family,
                           const std::vector<std::string>& styles,
                           bool                            loadIfNotFound)
{
   const std::string styleString = fmt::format("{}", fmt::join(styles, " "));
   const std::string fontString  = fmt::format("{}:{}", family, styleString);

   logger_->debug("LoadFontResource: {}", fontString);

   FontRecord fontRecord = Impl::MatchFontFile(family, styles);

   ImFontAtlas* atlas = Impl::CurrentAtlas();

   // Search for a loaded ImGui font
   {
      std::shared_lock imguiFontLock {p->imguiFontsMutex_};

      // Search for the associated ImGui font in the current atlas
      auto atlasIt = p->imguiFontsByAtlas_.find(atlas);
      if (atlasIt != p->imguiFontsByAtlas_.end())
      {
         auto it = atlasIt->second.find(fontRecord);
         if (it != atlasIt->second.end())
         {
            return it->second;
         }
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
   auto& atlasFonts = p->imguiFontsByAtlas_[atlas];
   auto  it         = atlasFonts.find(fontRecord);
   if (it != atlasFonts.end())
   {
      return it->second;
   }

   logger_->debug("Creating ImGui font for atlas {}: {}",
                  static_cast<void*>(atlas),
                  fontString);

   // Define a name for the ImGui font
   std::string fontName;
   try
   {
      fontName = fmt::format(
         "{}", std::filesystem::path(fontRecord.filename_).filename().string());
   }
   catch (const std::exception& ex)
   {
      logger_->warn(ex.what());
      fontName = fmt::format("{}", fontRecord.filename_);
   }

   // Create an ImGui font
   std::shared_ptr<types::ImGuiFont> imguiFont =
      std::make_shared<types::ImGuiFont>(fontName, rawFontData);

   // Store the ImGui font
   atlasFonts.insert_or_assign(fontRecord, imguiFont);

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

void FontManager::Impl::InitializeEnvironment()
{
#if defined(__linux__)
   // Because of the way Fontconfig is built with Conan, FONTCONFIG_PATH must be
   // defined on Linux to ensure fonts can be found
   static const std::string kFontconfigPathKey {"FONTCONFIG_PATH"};

   std::string fontconfigPath = scwx::util::GetEnvironment(kFontconfigPathKey);
   if (fontconfigPath.empty())
   {
      scwx::util::SetEnvironment(kFontconfigPathKey, "/etc/fonts");
   }
#endif
}

void FontManager::Impl::InitializeFontCache()
{
   const std::filesystem::path cachePath {main::ApplicationPaths::GetLocation(
      main::ApplicationPaths::StandardLocation::FontCache)};

   if (std::filesystem::exists(cachePath))
   {
      fontCachePath_ = cachePath.generic_string() + "/";
   }
   else
   {
      logger_->error("Font cache path does not exist: {}", cachePath.string());
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

bool FontManager::Impl::CheckFontFormat(const FcChar8* format)
{
   const std::string stdFormat = reinterpret_cast<const char*>(format);

   return stdFormat == kFcTrueType_ || stdFormat == kFcOpenType_;
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
   FcPatternAddBool(pattern, FC_SYMBOL, FcFalse);

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
   FcResult   result {};
   FcFontSet* matches = FcFontSort(nullptr, pattern, FcFalse, nullptr, &result);
   FontRecord record {};

   if (matches != nullptr)
   {
      for (int i = 0; i < matches->nfont; i++)
      {
         FcPattern* match =
            // Using C code requires pointer arithmetic
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            FcFontRenderPrepare(nullptr, pattern, matches->fonts[i]);
         if (match == nullptr)
         {
            continue;
         }
         FcChar8* fcFamily = nullptr;
         FcChar8* fcStyle  = nullptr;
         FcChar8* fcFile   = nullptr;
         FcChar8* fcFormat = nullptr;
         FcBool   fcSymbol = FcFalse;

         // Match was found, get properties
         if (FcPatternGetString(match, FC_FAMILY, 0, &fcFamily) ==
                FcResultMatch &&
             FcPatternGetString(match, FC_STYLE, 0, &fcStyle) ==
                FcResultMatch &&
             FcPatternGetString(match, FC_FILE, 0, &fcFile) == FcResultMatch &&
             FcPatternGetBool(match, FC_SYMBOL, 0, &fcSymbol) ==
                FcResultMatch &&
             FcPatternGetString(match, FC_FONTFORMAT, 0, &fcFormat) ==
                FcResultMatch &&
             fcSymbol == FcFalse /*Must check fcSymbol manually*/ &&
             CheckFontFormat(fcFormat))
         {
            record.family_   = reinterpret_cast<char*>(fcFamily);
            record.style_    = reinterpret_cast<char*>(fcStyle);
            record.filename_ = reinterpret_cast<char*>(fcFile);

            logger_->debug("Found matching font: {}:{} ({}) {}",
                           record.family_,
                           record.style_,
                           record.filename_,
                           fcSymbol);
            FcPatternDestroy(match);
            break;
         }

         FcPatternDestroy(match);
      }
   }

   if (record.filename_.empty())
   {
      logger_->warn("Could not find matching font: {}", fontString);
   }

   // Cleanup
   FcFontSetDestroy(matches);
   FcPatternDestroy(pattern);

   return record;
}

FontManager& FontManager::Instance()
{
   static FontManager instance_ {};
   return instance_;
}

size_t FontRecordHash<FontRecord>::operator()(const FontRecord& x) const
{
   size_t seed = 0;
   boost::hash_combine(seed, x.family_);
   boost::hash_combine(seed, x.style_);
   boost::hash_combine(seed, x.filename_);
   return seed;
}

bool operator==(const FontRecord& lhs, const FontRecord& rhs)
{
   return lhs.family_ == rhs.family_ && //
          lhs.style_ == rhs.style_ &&   //
          lhs.filename_ == rhs.filename_;
}

} // namespace scwx::qt::manager
