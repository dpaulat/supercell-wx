#include <scwx/qt/settings/text_settings.hpp>
#include <scwx/qt/types/text_types.hpp>

#include <boost/algorithm/string.hpp>

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

static const std::string kRegular_ {"Regular"};

static const std::unordered_map<types::FontCategory, std::string>
   kDefaultFontFamily_ {
      {types::FontCategory::Default, kAlteDIN1451Mittelscrhift_},
      {types::FontCategory::Tooltip, kInconsolata_}};
static const std::unordered_map<types::FontCategory, std::string>
   kDefaultFontStyle_ {{types::FontCategory::Default, kRegular_},
                       {types::FontCategory::Tooltip, kRegular_}};
static const std::unordered_map<types::FontCategory, double>
   kDefaultFontPointSize_ {{types::FontCategory::Default, 12.0},
                           {types::FontCategory::Tooltip, 10.5}};

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

   TextSettings* self_;

   std::unordered_map<types::FontCategory, FontData> fontData_ {};

   SettingsVariable<std::int64_t> hoverTextWrap_ {"hover_text_wrap"};
   SettingsVariable<std::string>  tooltipMethod_ {"tooltip_method"};
};

TextSettings::TextSettings() :
    SettingsCategory("text"), p(std::make_unique<Impl>(this))
{
   RegisterVariables({&p->hoverTextWrap_, &p->tooltipMethod_});
   SetDefaults();
}
TextSettings::~TextSettings() = default;

TextSettings::TextSettings(TextSettings&&) noexcept            = default;
TextSettings& TextSettings::operator=(TextSettings&&) noexcept = default;

void TextSettings::Impl::InitializeFontVariables()
{
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

      // TODO: Variable registration
   }
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

SettingsVariable<std::string>& TextSettings::tooltip_method() const
{
   return p->tooltipMethod_;
}

TextSettings& TextSettings::Instance()
{
   static TextSettings TextSettings_;
   return TextSettings_;
}

bool operator==(const TextSettings& lhs, const TextSettings& rhs)
{
   return (lhs.p->hoverTextWrap_ == rhs.p->hoverTextWrap_ &&
           lhs.p->tooltipMethod_ == rhs.p->tooltipMethod_);
}

} // namespace settings
} // namespace qt
} // namespace scwx
