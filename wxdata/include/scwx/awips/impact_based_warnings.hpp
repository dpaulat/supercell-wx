#pragma once

#include <scwx/awips/phenomenon.hpp>

#include <string>
#include <vector>

namespace scwx
{
namespace awips
{

enum class ThreatCategory : int
{
   Base         = 0,
   Significant  = 1,
   Considerable = 2,
   Destructive  = 3,
   Catastrophic = 4,
   Unknown
};

struct ImpactBasedWarningInfo
{
   bool                        hasObservedTag_ {false};
   bool                        hasTornadoPossibleTag_ {false};
   std::vector<ThreatCategory> threatCategories_ {ThreatCategory::Base};
};

const ImpactBasedWarningInfo& GetImpactBasedWarningInfo(Phenomenon phenomenon);

ThreatCategory     GetThreatCategory(const std::string& name);
const std::string& GetThreatCategoryName(ThreatCategory threatCategory);

} // namespace awips
} // namespace scwx
