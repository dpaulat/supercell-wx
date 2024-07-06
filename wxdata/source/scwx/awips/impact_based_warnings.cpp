#include <scwx/awips/impact_based_warnings.hpp>
#include <scwx/util/enum.hpp>

#include <unordered_map>

#include <boost/algorithm/string.hpp>

namespace scwx
{
namespace awips
{

static const std::string logPrefix_ = "scwx::awips::impact_based_warnings";

static const std::unordered_map<ThreatCategory, std::string>
   threatCategoryName_ {{ThreatCategory::Base, "Base"},
                        {ThreatCategory::Significant, "Significant"},
                        {ThreatCategory::Considerable, "Considerable"},
                        {ThreatCategory::Destructive, "Destructive"},
                        {ThreatCategory::Catastrophic, "Catastrophic"},
                        {ThreatCategory::Unknown, "?"}};

SCWX_GET_ENUM(ThreatCategory, GetThreatCategory, threatCategoryName_)

const std::string& GetThreatCategoryName(ThreatCategory threatCategory)
{
   return threatCategoryName_.at(threatCategory);
}

} // namespace awips
} // namespace scwx
