#include <scwx/qt/settings/button_settings.hpp>
#include <scwx/qt/util/color.hpp>

namespace scwx::qt::settings
{

static const std::string logPrefix_ = "scwx::qt::settings::button_settings";

// These defaults match ImGui defaults
static const boost::gil::rgba8_pixel_t kDefaultButtonColor_ {66, 150, 250, 102};
static const std::string               kDefaultButtonColorString_ {
   util::color::ToArgbString(kDefaultButtonColor_)};

static const boost::gil::rgba8_pixel_t kDefaultHoverColor_ {66, 150, 250, 255};
static const std::string               kDefaultHoverColorString_ {
   util::color::ToArgbString(kDefaultHoverColor_)};

static const boost::gil::rgba8_pixel_t kDefaultActiveColor_ {15, 135, 250, 255};
static const std::string               kDefaultActiveColorString_ {
   util::color::ToArgbString(kDefaultActiveColor_)};

class ButtonSettings::Impl
{
public:
   explicit Impl()
   {
      buttonColor_.SetDefault(kDefaultButtonColorString_);
      hoverColor_.SetDefault(kDefaultHoverColorString_);
      activeColor_.SetDefault(kDefaultActiveColorString_);

      buttonColor_.SetValidator(&util::color::ValidateArgbString);
      hoverColor_.SetValidator(&util::color::ValidateArgbString);
      activeColor_.SetValidator(&util::color::ValidateArgbString);
   }

   ~Impl()                       = default;
   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   SettingsVariable<std::string> buttonColor_ {"button_color"};
   SettingsVariable<std::string> hoverColor_ {"hover_color"};
   SettingsVariable<std::string> activeColor_ {"active_color"};
};

ButtonSettings::ButtonSettings(const std::string& name) :
    SettingsCategory(name), p(std::make_unique<Impl>())
{
   RegisterVariables({&p->buttonColor_, &p->hoverColor_, &p->activeColor_});
   SetDefaults();
}
ButtonSettings::~ButtonSettings() = default;

ButtonSettings::ButtonSettings(ButtonSettings&&) noexcept            = default;
ButtonSettings& ButtonSettings::operator=(ButtonSettings&&) noexcept = default;

SettingsVariable<std::string>& ButtonSettings::button_color() const
{
   return p->buttonColor_;
}

SettingsVariable<std::string>& ButtonSettings::hover_color() const
{
   return p->hoverColor_;
}

SettingsVariable<std::string>& ButtonSettings::active_color() const
{
   return p->activeColor_;
}

boost::gil::rgba8_pixel_t ButtonSettings::GetButtonColorRgba8() const
{
   return util::color::ToRgba8PixelT(p->buttonColor_.GetValue());
}

boost::gil::rgba8_pixel_t ButtonSettings::GetHoverColorRgba8() const
{
   return util::color::ToRgba8PixelT(p->hoverColor_.GetValue());
}

boost::gil::rgba8_pixel_t ButtonSettings::GetActiveColorRgba8() const
{
   return util::color::ToRgba8PixelT(p->activeColor_.GetValue());
}

boost::gil::rgba32f_pixel_t ButtonSettings::GetButtonColorRgba32f() const
{
   return util::color::ToRgba32fPixelT(p->buttonColor_.GetValue());
}

boost::gil::rgba32f_pixel_t ButtonSettings::GetHoverColorRgba32f() const
{
   return util::color::ToRgba32fPixelT(p->hoverColor_.GetValue());
}

boost::gil::rgba32f_pixel_t ButtonSettings::GetActiveColorRgba32f() const
{
   return util::color::ToRgba32fPixelT(p->activeColor_.GetValue());
}

void ButtonSettings::StageValues(const boost::gil::rgba8_pixel_t& buttonColor,
                                 const boost::gil::rgba8_pixel_t& hoverColor,
                                 const boost::gil::rgba8_pixel_t& activeColor)
{
   set_block_signals(true);

   p->buttonColor_.StageValue(util::color::ToArgbString(buttonColor));
   p->hoverColor_.StageValue(util::color::ToArgbString(hoverColor));
   p->activeColor_.StageValue(util::color::ToArgbString(activeColor));

   set_block_signals(false);

   staged_signal()();
}

bool operator==(const ButtonSettings& lhs, const ButtonSettings& rhs)
{
   return (lhs.p->buttonColor_ == rhs.p->buttonColor_ &&
           lhs.p->hoverColor_ == rhs.p->hoverColor_ &&
           lhs.p->activeColor_ == rhs.p->activeColor_);
}

} // namespace scwx::qt::settings
