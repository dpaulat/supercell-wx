#include <scwx/qt/settings/alert_palette_settings.hpp>
#include <scwx/qt/util/color.hpp>

#include <map>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/gil.hpp>
#include <boost/unordered/unordered_flat_map.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ =
   "scwx::qt::settings::alert_palette_settings";

static const boost::gil::rgba8_pixel_t kColorBlack_ {0, 0, 0, 255};

struct LineData
{
   boost::gil::rgba8_pixel_t borderColor_ {kColorBlack_};
   boost::gil::rgba8_pixel_t highlightColor_ {kColorBlack_};
   boost::gil::rgba8_pixel_t lineColor_;
   std::int64_t              borderWidth_ {1};
   std::int64_t              highlightWidth_ {0};
   std::int64_t              lineWidth_ {3};
};

typedef boost::unordered_flat_map<awips::ibw::ThreatCategory, LineData>
   ThreatCategoryPalette;

static const boost::unordered_flat_map<awips::Phenomenon, ThreatCategoryPalette>
   kThreatCategoryPalettes_ //
   {{awips::Phenomenon::Marine,
     {{awips::ibw::ThreatCategory::Base, {.lineColor_ {255, 127, 0, 255}}}}},
    {awips::Phenomenon::FlashFlood,
     {{awips::ibw::ThreatCategory::Base, {.lineColor_ {0, 255, 0, 255}}},
      {awips::ibw::ThreatCategory::Considerable,
       {.highlightColor_ {0, 255, 0, 255},
        .lineColor_ {kColorBlack_},
        .highlightWidth_ {1},
        .lineWidth_ {1}}},
      {awips::ibw::ThreatCategory::Catastrophic,
       {.highlightColor_ {0, 255, 0, 255},
        .lineColor_ {255, 0, 0, 255},
        .highlightWidth_ {1},
        .lineWidth_ {1}}}}},
    {awips::Phenomenon::SevereThunderstorm,
     {{awips::ibw::ThreatCategory::Base, {.lineColor_ {255, 255, 0, 255}}},
      {awips::ibw::ThreatCategory::Considerable,
       {.highlightColor_ {255, 255, 0, 255},
        .lineColor_ {255, 0, 0, 255},
        .highlightWidth_ {1},
        .lineWidth_ {1}}},
      {awips::ibw::ThreatCategory::Destructive,
       {.highlightColor_ {255, 255, 0, 255},
        .lineColor_ {255, 0, 0, 255},
        .highlightWidth_ {1},
        .lineWidth_ {2}}}}},
    {awips::Phenomenon::SnowSquall,
     {{awips::ibw::ThreatCategory::Base, {.lineColor_ {0, 255, 255, 255}}}}},
    {awips::Phenomenon::Tornado,
     {{awips::ibw::ThreatCategory::Base, {.lineColor_ {255, 0, 0, 255}}},
      {awips::ibw::ThreatCategory::Considerable,
       {.lineColor_ {255, 0, 255, 255}}},
      {awips::ibw::ThreatCategory::Catastrophic,
       {.highlightColor_ {255, 0, 255, 255},
        .lineColor_ {kColorBlack_},
        .highlightWidth_ {1},
        .lineWidth_ {1}}}}}};

static const boost::unordered_flat_map<awips::Phenomenon, LineData>
   kObservedPalettes_ //
   {{awips::Phenomenon::Tornado,
     {.highlightColor_ {255, 0, 0, 255},
      .lineColor_ {kColorBlack_},
      .highlightWidth_ {1},
      .lineWidth_ {1}}}};

static const boost::unordered_flat_map<awips::Phenomenon, LineData>
   kTornadoPossiblePalettes_ //
   {{awips::Phenomenon::Marine,
     {.highlightColor_ {255, 127, 0, 255},
      .lineColor_ {kColorBlack_},
      .highlightWidth_ {1},
      .lineWidth_ {1}}},
    {awips::Phenomenon::SevereThunderstorm,
     {.highlightColor_ {255, 255, 0, 255},
      .lineColor_ {kColorBlack_},
      .highlightWidth_ {1},
      .lineWidth_ {1}}}};

static const boost::unordered_flat_map<awips::Phenomenon, LineData>
   kInactivePalettes_ //
   {
      {awips::Phenomenon::Marine, {.lineColor_ {127, 63, 0, 255}}},
      {awips::Phenomenon::FlashFlood, {.lineColor_ {0, 127, 0, 255}}},
      {awips::Phenomenon::SevereThunderstorm, {.lineColor_ {127, 127, 0, 255}}},
      {awips::Phenomenon::SnowSquall, {.lineColor_ {0, 127, 127, 255}}},
      {awips::Phenomenon::Tornado, {.lineColor_ {127, 0, 0, 255}}},
   };

class AlertPaletteSettings::Impl
{
public:
   explicit Impl(awips::Phenomenon phenomenon) : phenomenon_ {phenomenon}
   {
      const auto& info = awips::ibw::GetImpactBasedWarningInfo(phenomenon);

      const auto& threatCategoryPalettes =
         kThreatCategoryPalettes_.at(phenomenon);

      for (auto& threatCategory : info.threatCategories_)
      {
         std::string threatCategoryName =
            awips::ibw::GetThreatCategoryName(threatCategory);
         boost::algorithm::to_lower(threatCategoryName);
         auto result =
            threatCategoryMap_.emplace(threatCategory, threatCategoryName);
         auto& lineSettings = result.first->second;

         SetDefaultLineData(lineSettings,
                            threatCategoryPalettes.at(threatCategory));
      }

      if (info.hasObservedTag_)
      {
         SetDefaultLineData(observed_, kObservedPalettes_.at(phenomenon));
      }

      if (info.hasTornadoPossibleTag_)
      {
         SetDefaultLineData(tornadoPossible_,
                            kTornadoPossiblePalettes_.at(phenomenon));
      }

      SetDefaultLineData(inactive_, kInactivePalettes_.at(phenomenon));
   }
   ~Impl() {}

   static void SetDefaultLineData(LineSettings&   lineSettings,
                                  const LineData& lineData);

   awips::Phenomenon phenomenon_;

   std::map<awips::ibw::ThreatCategory, LineSettings> threatCategoryMap_ {};

   LineSettings observed_ {"observed"};
   LineSettings tornadoPossible_ {"tornado_possible"};
   LineSettings inactive_ {"inactive"};
};

AlertPaletteSettings::AlertPaletteSettings(awips::Phenomenon phenomenon) :
    SettingsCategory(awips::GetPhenomenonCode(phenomenon)),
    p(std::make_unique<Impl>(phenomenon))
{
   auto& info = awips::ibw::GetImpactBasedWarningInfo(p->phenomenon_);
   for (auto& threatCategory : p->threatCategoryMap_)
   {
      RegisterSubcategory(threatCategory.second);
   }

   if (info.hasObservedTag_)
   {
      RegisterSubcategory(p->observed_);
   }

   if (info.hasTornadoPossibleTag_)
   {
      RegisterSubcategory(p->tornadoPossible_);
   }

   RegisterSubcategory(p->inactive_);

   SetDefaults();
}

AlertPaletteSettings::~AlertPaletteSettings() = default;

AlertPaletteSettings::AlertPaletteSettings(AlertPaletteSettings&&) noexcept =
   default;
AlertPaletteSettings&
AlertPaletteSettings::operator=(AlertPaletteSettings&&) noexcept = default;

LineSettings& AlertPaletteSettings::threat_category(
   awips::ibw::ThreatCategory threatCategory) const
{
   auto it = p->threatCategoryMap_.find(threatCategory);
   if (it != p->threatCategoryMap_.cend())
   {
      return it->second;
   }
   return p->threatCategoryMap_.at(awips::ibw::ThreatCategory::Base);
}

LineSettings& AlertPaletteSettings::inactive() const
{
   return p->inactive_;
}

LineSettings& AlertPaletteSettings::observed() const
{
   return p->observed_;
}

LineSettings& AlertPaletteSettings::tornado_possible() const
{
   return p->tornadoPossible_;
}

void AlertPaletteSettings::Impl::SetDefaultLineData(LineSettings& lineSettings,
                                                    const LineData& lineData)
{
   lineSettings.border_color().SetDefault(
      util::color::ToArgbString(lineData.borderColor_));
   lineSettings.highlight_color().SetDefault(
      util::color::ToArgbString(lineData.highlightColor_));
   lineSettings.line_color().SetDefault(
      util::color::ToArgbString(lineData.lineColor_));

   lineSettings.border_width().SetDefault(lineData.borderWidth_);
   lineSettings.highlight_width().SetDefault(lineData.highlightWidth_);
   lineSettings.line_width().SetDefault(lineData.lineWidth_);
}

bool operator==(const AlertPaletteSettings& lhs,
                const AlertPaletteSettings& rhs)
{
   return (lhs.p->phenomenon_ == rhs.p->phenomenon_ &&
           lhs.p->threatCategoryMap_ == rhs.p->threatCategoryMap_ &&
           lhs.p->inactive_ == rhs.p->inactive_ &&
           lhs.p->observed_ == rhs.p->observed_ &&
           lhs.p->tornadoPossible_ == rhs.p->tornadoPossible_);
}

} // namespace settings
} // namespace qt
} // namespace scwx
