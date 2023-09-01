#include <scwx/qt/settings/text_settings.hpp>

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
      hoverTextWrap_.SetDefault(80);
      hoverTextWrap_.SetMinimum(0);
      hoverTextWrap_.SetMaximum(999);
   }

   ~Impl() {}

   SettingsVariable<std::int64_t> hoverTextWrap_ {"hover_text_wrap"};
};

TextSettings::TextSettings() :
    SettingsCategory("text"), p(std::make_unique<Impl>())
{
   RegisterVariables({&p->hoverTextWrap_});
   SetDefaults();
}
TextSettings::~TextSettings() = default;

TextSettings::TextSettings(TextSettings&&) noexcept            = default;
TextSettings& TextSettings::operator=(TextSettings&&) noexcept = default;

SettingsVariable<std::int64_t>& TextSettings::hover_text_wrap() const
{
   return p->hoverTextWrap_;
}

TextSettings& TextSettings::Instance()
{
   static TextSettings TextSettings_;
   return TextSettings_;
}

bool operator==(const TextSettings& lhs, const TextSettings& rhs)
{
   return (lhs.p->hoverTextWrap_ == rhs.p->hoverTextWrap_);
}

} // namespace settings
} // namespace qt
} // namespace scwx
