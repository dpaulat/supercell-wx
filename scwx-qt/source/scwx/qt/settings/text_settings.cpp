#if defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wdelete-non-abstract-non-virtual-dtor"
#endif

#include <scwx/qt/settings/text_settings.hpp>
#include <scwx/qt/types/text_types.hpp>

#include <boost/algorithm/string.hpp>

#if defined(__clang__)
#   pragma clang diagnostic pop
#endif

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ = "scwx::qt::settings::text_settings";

static const std::string kAlteDIN1451Mittelscrhift_ {
   "Alte DIN 1451 Mittelschrift"};
static const std::string kInconsolata_ {"Inconsolata"};
static const std::string kRobotoFlex_ {"Roboto Flex"};

static const std::string kRegular_ {"Regular"};

static const std::unordered_map<types::FontCategory, std::string>
   kDefaultFontFamily_ {
      {types::FontCategory::Default, kAlteDIN1451Mittelscrhift_},
      {types::FontCategory::Tooltip, kInconsolata_},
      {types::FontCategory::Attribution, kRobotoFlex_}};
static const std::unordered_map<types::FontCategory, std::string>
   kDefaultFontStyle_ {{types::FontCategory::Default, kRegular_},
                       {types::FontCategory::Tooltip, kRegular_},
                       {types::FontCategory::Attribution, kRegular_}};
static const std::unordered_map<types::FontCategory, double>
   kDefaultFontPointSize_ {{types::FontCategory::Default, 12.0},
                           {types::FontCategory::Tooltip, 10.5},
                           {types::FontCategory::Attribution, 9.0}};

class TextSettings::Impl
{
public:
   struct FontData
   {
      SettingsVariable<std::string> fontFamily_ {"font_family"};
      SettingsVariable<std::string> fontStyle_ {"font_style"};
      SettingsVariable<double>      fontPointSize_ {"font_point_size"};
   };

   explicit Impl(TextSettings* self) : self_ {self}
   {
      std::string defaultTooltipMethodValue =
         types::GetTooltipMethodName(types::TooltipMethod::ImGui);

      boost::to_lower(defaultTooltipMethodValue);

      hoverTextWrap_.SetDefault(80);
      hoverTextWrap_.SetMinimum(0);
      hoverTextWrap_.SetMaximum(999);
      placefileTextDropShadowEnabled_.SetDefault(true);
      radarSiteHoverTextEnabled_.SetDefault(true);
      tooltipMethod_.SetDefault(defaultTooltipMethodValue);

      tooltipMethod_.SetValidator(
         [](const std::string& value)
         {
            for (types::TooltipMethod tooltipMethod :
                 types::TooltipMethodIterator())
            {
               // If the value is equal to a lower case alert action name
               std::string tooltipMethodName =
                  types::GetTooltipMethodName(tooltipMethod);
               boost::to_lower(tooltipMethodName);
               if (value == tooltipMethodName)
               {
                  // Regard as a match, valid
                  return true;
               }
            }

            // No match found, invalid
            return false;
         });

      InitializeFontVariables();
   }

   ~Impl() {}

   void InitializeFontVariables();

   friend bool operator==(const FontData& lhs, const FontData& rhs)
   {
      return (lhs.fontFamily_ == rhs.fontFamily_ &&
              lhs.fontStyle_ == rhs.fontStyle_ &&
              lhs.fontPointSize_ == rhs.fontPointSize_);
   }

   TextSettings* self_;

   std::unordered_map<types::FontCategory, FontData> fontData_ {};
   std::vector<SettingsCategory>                     fontSettings_ {};

   SettingsVariable<std::int64_t> hoverTextWrap_ {"hover_text_wrap"};
   SettingsVariable<std::string>  tooltipMethod_ {"tooltip_method"};

   SettingsVariable<bool> placefileTextDropShadowEnabled_ {
      "placefile_text_drop_shadow_enabled"};
   SettingsVariable<bool> radarSiteHoverTextEnabled_ {
      "radar_site_hover_text_enabled"};
};

TextSettings::TextSettings() :
    SettingsCategory("text"), p(std::make_unique<Impl>(this))
{
   RegisterVariables({&p->hoverTextWrap_,
                      &p->placefileTextDropShadowEnabled_,
                      &p->radarSiteHoverTextEnabled_,
                      &p->tooltipMethod_});
   SetDefaults();
}
TextSettings::~TextSettings() = default;

TextSettings::TextSettings(TextSettings&&) noexcept            = default;
TextSettings& TextSettings::operator=(TextSettings&&) noexcept = default;

void TextSettings::Impl::InitializeFontVariables()
{
   fontData_.reserve(types::FontCategoryIterator().count());

   for (auto fontCategory : types::FontCategoryIterator())
   {
      auto  result = fontData_.emplace(fontCategory, FontData {});
      auto& pair   = *result.first;
      auto& font   = pair.second;

      font.fontFamily_.SetDefault(kDefaultFontFamily_.at(fontCategory));
      font.fontStyle_.SetDefault(kDefaultFontStyle_.at(fontCategory));
      font.fontPointSize_.SetDefault(kDefaultFontPointSize_.at(fontCategory));

      // String values must not be empty
      font.fontFamily_.SetValidator([](const std::string& value)
                                    { return !value.empty(); });
      font.fontStyle_.SetValidator([](const std::string& value)
                                   { return !value.empty(); });

      // Font point size must be between 6 and 72
      font.fontPointSize_.SetMinimum(6.0);
      font.fontPointSize_.SetMaximum(72.0);

      // Variable registration
      auto& settings = fontSettings_.emplace_back(
         SettingsCategory {types::GetFontCategoryName(fontCategory)});

      settings.RegisterVariables(
         {&font.fontFamily_, &font.fontStyle_, &font.fontPointSize_});
   }

   self_->RegisterSubcategoryArray("fonts", fontSettings_);
}

SettingsVariable<std::string>&
TextSettings::font_family(types::FontCategory fontCategory) const
{
   return p->fontData_.at(fontCategory).fontFamily_;
}

SettingsVariable<std::string>&
TextSettings::font_style(types::FontCategory fontCategory) const
{
   return p->fontData_.at(fontCategory).fontStyle_;
}

SettingsVariable<double>&
TextSettings::font_point_size(types::FontCategory fontCategory) const
{
   return p->fontData_.at(fontCategory).fontPointSize_;
}

SettingsVariable<std::int64_t>& TextSettings::hover_text_wrap() const
{
   return p->hoverTextWrap_;
}

SettingsVariable<bool>& TextSettings::placefile_text_drop_shadow_enabled() const
{
   return p->placefileTextDropShadowEnabled_;
}

SettingsVariable<bool>& TextSettings::radar_site_hover_text_enabled() const
{
   return p->radarSiteHoverTextEnabled_;
}

SettingsVariable<std::string>& TextSettings::tooltip_method() const
{
   return p->tooltipMethod_;
}

TextSettings& TextSettings::Instance()
{
   static TextSettings textSettings_;
   return textSettings_;
}

bool operator==(const TextSettings& lhs, const TextSettings& rhs)
{
   return (lhs.p->fontData_ == rhs.p->fontData_ &&
           lhs.p->hoverTextWrap_ == rhs.p->hoverTextWrap_ &&
           lhs.p->placefileTextDropShadowEnabled_ ==
              rhs.p->placefileTextDropShadowEnabled_ &&
           lhs.p->radarSiteHoverTextEnabled_ ==
              rhs.p->radarSiteHoverTextEnabled_ &&
           lhs.p->tooltipMethod_ == rhs.p->tooltipMethod_);
}

} // namespace settings
} // namespace qt
} // namespace scwx
