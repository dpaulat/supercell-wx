#include <scwx/spc/spc_types.hpp>

#include <unordered_map>

namespace scwx::spc
{

static const std::unordered_map<OutlookDay, std::string> outlookDayName_ {
   {OutlookDay::Day1, "Day 1"},
   {OutlookDay::Day2, "Day 2"},
   {OutlookDay::Day3, "Day 3"},
   {OutlookDay::Unknown, "?"}};

static const std::unordered_map<OutlookProduct, std::string>
   outlookProductName_ {
      {OutlookProduct::Categorical, "Categorical"},
      {OutlookProduct::Tornado, "Tornado"},
      {OutlookProduct::Wind, "Wind"},
      {OutlookProduct::Hail, "Hail"},
      {OutlookProduct::Probabilistic, "Probabilistic"},
      {OutlookProduct::SignificantProbabilistic, "Significant Probabilistic"},
      {OutlookProduct::Unknown, "?"}};

static const std::unordered_map<CategoricalRisk, std::string>
   categoricalRiskName_ {
      {CategoricalRisk::GeneralThunderstorm, "General Thunderstorm"},
      {CategoricalRisk::Marginal, "Marginal"},
      {CategoricalRisk::Slight, "Slight"},
      {CategoricalRisk::Enhanced, "Enhanced"},
      {CategoricalRisk::Moderate, "Moderate"},
      {CategoricalRisk::High, "High"},
      {CategoricalRisk::Unknown, "?"}};

std::string GetOutlookDayName(OutlookDay day)
{
   return outlookDayName_.at(day);
}

std::string GetOutlookProductName(OutlookProduct product)
{
   return outlookProductName_.at(product);
}

std::string GetCategoricalRiskName(CategoricalRisk risk)
{
   return categoricalRiskName_.at(risk);
}

OutlookDay GetOutlookDay(const std::string& name)
{
   auto result =
      std::find_if(outlookDayName_.cbegin(),
                   outlookDayName_.cend(),
                   [&](const auto& pair) { return pair.second == name; });

   if (result != outlookDayName_.cend())
   {
      return result->first;
   }
   return OutlookDay::Unknown;
}

OutlookProduct GetOutlookProduct(const std::string& name)
{
   auto result =
      std::find_if(outlookProductName_.cbegin(),
                   outlookProductName_.cend(),
                   [&](const auto& pair) { return pair.second == name; });

   if (result != outlookProductName_.cend())
   {
      return result->first;
   }
   return OutlookProduct::Unknown;
}

CategoricalRisk GetCategoricalRisk(int32_t dn)
{
   switch (dn)
   {
   case 2:
      return CategoricalRisk::GeneralThunderstorm;
   case 3:
      return CategoricalRisk::Marginal;
   case 4:
      return CategoricalRisk::Slight;
   case 5:
      return CategoricalRisk::Enhanced;
   case 6:
      return CategoricalRisk::Moderate;
   case 8:
      return CategoricalRisk::High;
   default:
      return CategoricalRisk::Unknown;
   }
}

} // namespace scwx::spc
