#include <scwx/qt/settings/alert_palette_settings.hpp>
#include <scwx/qt/util/color.hpp>

#include <map>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/gil.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ =
   "scwx::qt::settings::alert_palette_settings";

class AlertPaletteSettings::Impl
{
public:
   explicit Impl(awips::Phenomenon phenomenon) : phenomenon_ {phenomenon}
   {
      auto& info = awips::ibw::GetImpactBasedWarningInfo(phenomenon);
      for (auto& threatCategory : info.threatCategories_)
      {
         std::string threatCategoryName =
            awips::ibw::GetThreatCategoryName(threatCategory);
         boost::algorithm::to_lower(threatCategoryName);
         threatCategoryMap_.emplace(threatCategory, threatCategoryName);
      }
   }
   ~Impl() {}

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

bool operator==(const AlertPaletteSettings& lhs,
                const AlertPaletteSettings& rhs)
{
   return (lhs.p->threatCategoryMap_ == rhs.p->threatCategoryMap_ &&
           lhs.p->inactive_ == rhs.p->inactive_ &&
           lhs.p->observed_ == rhs.p->observed_ &&
           lhs.p->tornadoPossible_ == rhs.p->tornadoPossible_);
}

} // namespace settings
} // namespace qt
} // namespace scwx
