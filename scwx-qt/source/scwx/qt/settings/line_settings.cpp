#include <scwx/qt/settings/line_settings.hpp>
#include <scwx/qt/util/color.hpp>

#include <boost/gil.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ = "scwx::qt::settings::line_settings";

static const boost::gil::rgba8_pixel_t kTransparentColor_ {0, 0, 0, 0};
static const std::string               kTransparentColorString_ {
   util::color::ToArgbString(kTransparentColor_)};

static const boost::gil::rgba8_pixel_t kBlackColor_ {0, 0, 0, 255};
static const std::string               kBlackColorString_ {
   util::color::ToArgbString(kBlackColor_)};

static const boost::gil::rgba8_pixel_t kWhiteColor_ {255, 255, 255, 255};
static const std::string               kWhiteColorString_ {
   util::color::ToArgbString(kWhiteColor_)};

class LineSettings::Impl
{
public:
   explicit Impl()
   {
      lineColor_.SetDefault(kWhiteColorString_);
      highlightColor_.SetDefault(kTransparentColorString_);
      borderColor_.SetDefault(kBlackColorString_);

      lineWidth_.SetDefault(3);
      highlightWidth_.SetDefault(0);
      borderWidth_.SetDefault(1);

      lineWidth_.SetMinimum(1);
      highlightWidth_.SetMinimum(0);
      borderWidth_.SetMinimum(0);

      lineWidth_.SetMaximum(9);
      highlightWidth_.SetMaximum(9);
      borderWidth_.SetMaximum(9);

      lineColor_.SetValidator(&util::color::ValidateArgbString);
      highlightColor_.SetValidator(&util::color::ValidateArgbString);
      borderColor_.SetValidator(&util::color::ValidateArgbString);
   }
   ~Impl() {}

   SettingsVariable<std::string> lineColor_ {"line_color"};
   SettingsVariable<std::string> highlightColor_ {"highlight_color"};
   SettingsVariable<std::string> borderColor_ {"border_color"};

   SettingsVariable<std::int64_t> lineWidth_ {"line_width"};
   SettingsVariable<std::int64_t> highlightWidth_ {"highlight_width"};
   SettingsVariable<std::int64_t> borderWidth_ {"border_width"};
};

LineSettings::LineSettings(const std::string& name) :
    SettingsCategory(name), p(std::make_unique<Impl>())
{
   RegisterVariables({&p->lineColor_,
                      &p->highlightColor_,
                      &p->borderColor_,
                      &p->lineWidth_,
                      &p->highlightWidth_,
                      &p->borderWidth_});
   SetDefaults();
}
LineSettings::~LineSettings() = default;

LineSettings::LineSettings(LineSettings&&) noexcept            = default;
LineSettings& LineSettings::operator=(LineSettings&&) noexcept = default;

SettingsVariable<std::string>& LineSettings::border_color() const
{
   return p->borderColor_;
}

SettingsVariable<std::string>& LineSettings::highlight_color() const
{
   return p->highlightColor_;
}

SettingsVariable<std::string>& LineSettings::line_color() const
{
   return p->lineColor_;
}

SettingsVariable<std::int64_t>& LineSettings::border_width() const
{
   return p->borderWidth_;
}

SettingsVariable<std::int64_t>& LineSettings::highlight_width() const
{
   return p->highlightWidth_;
}

SettingsVariable<std::int64_t>& LineSettings::line_width() const
{
   return p->lineWidth_;
}

bool operator==(const LineSettings& lhs, const LineSettings& rhs)
{
   return (lhs.p->borderColor_ == rhs.p->borderColor_ &&
           lhs.p->highlightColor_ == rhs.p->highlightColor_ &&
           lhs.p->lineColor_ == rhs.p->lineColor_ &&
           lhs.p->borderWidth_ == rhs.p->borderWidth_ &&
           lhs.p->highlightWidth_ == rhs.p->highlightWidth_ &&
           lhs.p->lineWidth_ == rhs.p->lineWidth_);
}

} // namespace settings
} // namespace qt
} // namespace scwx
