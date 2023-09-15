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

class TextSettings::Impl
{
public:
   explicit Impl()
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
   }

   ~Impl() {}

   SettingsVariable<std::int64_t> hoverTextWrap_ {"hover_text_wrap"};
   SettingsVariable<std::string>  tooltipMethod_ {"tooltip_method"};
};

TextSettings::TextSettings() :
    SettingsCategory("text"), p(std::make_unique<Impl>())
{
   RegisterVariables({&p->hoverTextWrap_, &p->tooltipMethod_});
   SetDefaults();
}
TextSettings::~TextSettings() = default;

TextSettings::TextSettings(TextSettings&&) noexcept            = default;
TextSettings& TextSettings::operator=(TextSettings&&) noexcept = default;

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
