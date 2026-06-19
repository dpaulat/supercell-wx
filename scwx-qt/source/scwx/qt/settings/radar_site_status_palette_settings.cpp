#include <scwx/qt/settings/radar_site_status_palette_settings.hpp>
#include <scwx/qt/util/color.hpp>

#include <boost/gil.hpp>
#include <boost/unordered/unordered_flat_map.hpp>

namespace scwx::qt::settings
{

static const std::string logPrefix_ =
   "scwx::qt::settings::radar_site_status_palette_settings";

static const boost::unordered_flat_map<types::RadarSiteStatus,
                                       std::tuple<boost::gil::rgba8_pixel_t,
                                                  boost::gil::rgba8_pixel_t,
                                                  boost::gil::rgba8_pixel_t>>
   kDefaultRadarSiteStatusButtonColors_ {
      // Colors: Button, Hover, Active
      {types::RadarSiteStatus::Up, // Green
       {{66, 250, 66, 102}, {39, 150, 39, 255}, {9, 150, 9, 255}}},
      {types::RadarSiteStatus::Warning, // Yellow
       {{248, 250, 66, 102}, {149, 150, 39, 255}, {148, 150, 9, 255}}},
      {types::RadarSiteStatus::Down, // Red
       {{250, 66, 66, 102}, {150, 39, 39, 255}, {150, 9, 9, 255}}},
      {types::RadarSiteStatus::HighLatency, // Orange
       {{250, 131, 66, 102}, {150, 78, 39, 255}, {150, 59, 9, 255}}},
      {types::RadarSiteStatus::Unknown, // Blue
       {{66, 150, 250, 102}, {66, 150, 250, 255}, {66, 150, 250, 255}}},
   };

class RadarSiteStatusPaletteSettings::Impl
{
public:
   explicit Impl(types::RadarSiteStatus status) : status_ {status}
   {
      SetDefaultButtonData();
   }

   ~Impl()                       = default;
   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void SetDefaultButtonData();

   types::RadarSiteStatus status_;

   ButtonSettings button_ {"button"};
};

RadarSiteStatusPaletteSettings::RadarSiteStatusPaletteSettings(
   types::RadarSiteStatus status) :
    SettingsCategory(types::GetRadarSiteStatusName(status)),
    p(std::make_unique<Impl>(status))
{
   RegisterSubcategory(p->button_);

   SetDefaults();
}

RadarSiteStatusPaletteSettings::~RadarSiteStatusPaletteSettings() = default;

RadarSiteStatusPaletteSettings::RadarSiteStatusPaletteSettings(
   RadarSiteStatusPaletteSettings&&) noexcept = default;
RadarSiteStatusPaletteSettings& RadarSiteStatusPaletteSettings::operator=(
   RadarSiteStatusPaletteSettings&&) noexcept = default;

ButtonSettings& RadarSiteStatusPaletteSettings::button() const
{
   return p->button_;
}

void RadarSiteStatusPaletteSettings::Impl::SetDefaultButtonData()
{
   auto& defaultColors = kDefaultRadarSiteStatusButtonColors_.at(status_);

   button_.button_color().SetDefault(
      util::color::ToArgbString(std::get<0>(defaultColors)));
   button_.hover_color().SetDefault(
      util::color::ToArgbString(std::get<1>(defaultColors)));
   button_.active_color().SetDefault(
      util::color::ToArgbString(std::get<2>(defaultColors)));
}

bool operator==(const RadarSiteStatusPaletteSettings& lhs,
                const RadarSiteStatusPaletteSettings& rhs)
{
   return (lhs.p->button_ == rhs.p->button_);
}

} // namespace scwx::qt::settings
