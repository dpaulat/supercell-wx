#include <scwx/awips/impact_based_warnings.hpp>
#include <scwx/util/enum.hpp>

#include <unordered_map>

#include <boost/algorithm/string.hpp>
#include <boost/unordered/unordered_flat_map.hpp>

namespace scwx
{
namespace awips
{
namespace ibw
{

static const std::string logPrefix_ = "scwx::awips::ibw::impact_based_warnings";

static const boost::unordered_flat_map<Phenomenon, ImpactBasedWarningInfo>
   impactBasedWarningInfo_ {
      {Phenomenon::Marine,
       ImpactBasedWarningInfo {.hasTornadoPossibleTag_ {true}}},
      {Phenomenon::FlashFlood,
       ImpactBasedWarningInfo {
          .threatCategories_ {ThreatCategory::Base,
                              ThreatCategory::Considerable,
                              ThreatCategory::Catastrophic}}},
      {Phenomenon::SevereThunderstorm,
       ImpactBasedWarningInfo {
          .hasTornadoPossibleTag_ {true},
          .threatCategories_ {ThreatCategory::Base,
                              ThreatCategory::Considerable,
                              ThreatCategory::Destructive}}},
      {Phenomenon::SnowSquall, ImpactBasedWarningInfo {}},
      {Phenomenon::Tornado,
       ImpactBasedWarningInfo {
          .hasObservedTag_ {true},
          .threatCategories_ {ThreatCategory::Base,
                              ThreatCategory::Considerable,
                              ThreatCategory::Catastrophic}}},
      {Phenomenon::Unknown, ImpactBasedWarningInfo {}}};

static const std::unordered_map<ThreatCategory, std::string>
   threatCategoryName_ {{ThreatCategory::Base, "Base"},
                        {ThreatCategory::Significant, "Significant"},
                        {ThreatCategory::Considerable, "Considerable"},
                        {ThreatCategory::Destructive, "Destructive"},
                        {ThreatCategory::Catastrophic, "Catastrophic"},
                        {ThreatCategory::Unknown, "?"}};

const ImpactBasedWarningInfo& GetImpactBasedWarningInfo(Phenomenon phenomenon)
{
   auto it = impactBasedWarningInfo_.find(phenomenon);
   if (it != impactBasedWarningInfo_.cend())
   {
      return it->second;
   }
   return impactBasedWarningInfo_.at(Phenomenon::Unknown);
}

SCWX_GET_ENUM(ThreatCategory, GetThreatCategory, threatCategoryName_)

const std::string& GetThreatCategoryName(ThreatCategory threatCategory)
{
   return threatCategoryName_.at(threatCategory);
}

} // namespace ibw
} // namespace awips
} // namespace scwx
