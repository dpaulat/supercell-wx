#include <scwx/awips/impact_based_warnings.hpp>
#include <scwx/util/enum.hpp>

#include <unordered_map>

#include <boost/algorithm/string.hpp>
#include <boost/unordered/unordered_flat_map.hpp>

namespace scwx
{
namespace awips
{

static const std::string logPrefix_ = "scwx::awips::impact_based_warnings";

static const boost::unordered_flat_map<Phenomenon, PhenomenonInfo>
   phenomenaInfo_ {
      {Phenomenon::Marine, PhenomenonInfo {.hasTornadoPossibleTag_ {true}}},
      {Phenomenon::FlashFlood,
       PhenomenonInfo {.threatCategories_ {ThreatCategory::Base,
                                           ThreatCategory::Considerable,
                                           ThreatCategory::Catastrophic}}},
      {Phenomenon::SevereThunderstorm,
       PhenomenonInfo {.hasTornadoPossibleTag_ {true},
                       .threatCategories_ {ThreatCategory::Base,
                                           ThreatCategory::Considerable,
                                           ThreatCategory::Destructive}}},
      {Phenomenon::SnowSquall, PhenomenonInfo {}},
      {Phenomenon::Tornado,
       PhenomenonInfo {.hasObservedTag_ {true},
                       .threatCategories_ {ThreatCategory::Base,
                                           ThreatCategory::Considerable,
                                           ThreatCategory::Catastrophic}}},
      {Phenomenon::Unknown, PhenomenonInfo {}}};

static const std::unordered_map<ThreatCategory, std::string>
   threatCategoryName_ {{ThreatCategory::Base, "Base"},
                        {ThreatCategory::Significant, "Significant"},
                        {ThreatCategory::Considerable, "Considerable"},
                        {ThreatCategory::Destructive, "Destructive"},
                        {ThreatCategory::Catastrophic, "Catastrophic"},
                        {ThreatCategory::Unknown, "?"}};

const PhenomenonInfo& GetPhenomenonInfo(Phenomenon phenomenon)
{
   auto it = phenomenaInfo_.find(phenomenon);
   if (it != phenomenaInfo_.cend())
   {
      return it->second;
   }
   return phenomenaInfo_.at(Phenomenon::Unknown);
}

SCWX_GET_ENUM(ThreatCategory, GetThreatCategory, threatCategoryName_)

const std::string& GetThreatCategoryName(ThreatCategory threatCategory)
{
   return threatCategoryName_.at(threatCategory);
}

} // namespace awips
} // namespace scwx
