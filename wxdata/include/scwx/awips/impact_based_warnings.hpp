#pragma once

#include <string>

namespace scwx
{
namespace awips
{

enum class ThreatCategory
{
   Base,
   Significant,
   Considerable,
   Destructive,
   Catastrophic,
   Unknown
};

ThreatCategory     GetThreatCategory(const std::string& name);
const std::string& GetThreatCategoryName(ThreatCategory threatCategory);

} // namespace awips
} // namespace scwx
