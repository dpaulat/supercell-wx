#pragma once

#include <scwx/util/iterator.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace scwx::spc
{

enum class OutlookDay
{
   Day1,
   Day2,
   Day3,
   Unknown
};
using OutlookDayIterator =
   util::Iterator<OutlookDay, OutlookDay::Day1, OutlookDay::Day3>;

enum class OutlookProduct
{
   Categorical,
   Tornado,
   Wind,
   Hail,
   Probabilistic,
   SignificantProbabilistic,
   Unknown
};
using OutlookProductIterator =
   util::Iterator<OutlookProduct,
                  OutlookProduct::Categorical,
                  OutlookProduct::SignificantProbabilistic>;

enum class CategoricalRisk
{
   GeneralThunderstorm = 2,
   Marginal            = 3,
   Slight              = 4,
   Enhanced            = 5,
   Moderate            = 6,
   High                = 8,
   Unknown             = 0
};

struct OutlookPolygon
{
   std::vector<std::vector<std::pair<double, double>>>
                   rings_;  // [ring][point] = (lat, lon)
   int32_t         dn_ {0}; // SPC DN value (risk code or probability)
   CategoricalRisk categoricalRisk_ {CategoricalRisk::Unknown};
   bool            isProbability_ {false};
   std::string     fillColor_ {};
   std::string     strokeColor_ {};
   int             cigLevel_ {
      0}; // Conditional Intensity Group: 0=none, 1=CIG1, 2=CIG2, 3=CIG3
};

struct OutlookData
{
   OutlookDay                  day_;
   OutlookProduct              product_;
   std::vector<OutlookPolygon> polygons_;
};

std::string GetOutlookDayName(OutlookDay day);
std::string GetOutlookProductName(OutlookProduct product);
std::string GetCategoricalRiskName(CategoricalRisk risk);

OutlookDay      GetOutlookDay(const std::string& name);
OutlookProduct  GetOutlookProduct(const std::string& name);
CategoricalRisk GetCategoricalRisk(int32_t dn);

} // namespace scwx::spc
