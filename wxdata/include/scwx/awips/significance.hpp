#pragma once

#include <string>

namespace scwx
{
namespace awips
{

enum class Significance
{
   Warning,
   Watch,
   Advisory,
   Statement,
   Forecast,
   Outlook,
   Synopsis,
   Unknown
};

Significance       GetSignificance(const std::string& code);
const std::string& GetSignificanceCode(Significance significance);
const std::string& GetSignificanceText(Significance significance);

} // namespace awips
} // namespace scwx
