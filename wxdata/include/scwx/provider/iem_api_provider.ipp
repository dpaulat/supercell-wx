#pragma once

#include <scwx/provider/iem_api_provider.hpp>
#include <scwx/network/cpr.hpp>
#include <scwx/util/time.hpp>

#include <boost/algorithm/string/join.hpp>
#include <cpr/cpr.h>
#include <range/v3/view/cartesian_product.hpp>
#include <range/v3/view/single.hpp>
#include <range/v3/view/transform.hpp>

#if (__cpp_lib_chrono < 201907L)
#   include <date/date.h>
#endif

namespace scwx::provider
{

template<ranges::forward_range DateRange,
         ranges::forward_range CcccRange,
         ranges::forward_range PilRange>
   requires std::same_as<ranges::range_value_t<DateRange>,
                         std::chrono::sys_days> &&
            std::same_as<ranges::range_value_t<CcccRange>, std::string_view> &&
            std::same_as<ranges::range_value_t<PilRange>, std::string_view>
boost::outcome_v2::result<std::vector<types::iem::AfosEntry>>
IemApiProvider::ListTextProducts(DateRange dates,
                                 CcccRange ccccs,
                                 PilRange  pils)
{
   using namespace std::chrono;

#if (__cpp_lib_chrono >= 201907L)
   namespace df = std;

   static constexpr std::string_view kDateFormat {"{:%Y-%m-%d}"};
#else
   using namespace date;
   namespace df = date;

#   define kDateFormat "%Y-%m-%d"
#endif

   auto formattedDates = dates | ranges::views::transform(
                                    [](const std::chrono::sys_days& date)
                                    { return df::format(kDateFormat, date); });

   logger_->debug("Listing text products for: {}",
                  boost::algorithm::join(formattedDates, ", "));

   std::vector<cpr::AsyncResponse> asyncResponses {};

   for (const auto& [date, cccc, pil] :
        ranges::views::cartesian_product(dates, ccccs, pils))
   {
      auto parameters =
         cpr::Parameters {{"date", df::format(kDateFormat, date)}};

      // WMO Source Code
      if (!cccc.empty())
      {
         parameters.Add({"cccc", std::string {cccc}});
      }

      // AFOS / AWIPS ID / 3-6 length identifier
      if (!pil.empty())
      {
         parameters.Add({"pil", std::string {pil}});
      }

      asyncResponses.emplace_back(
         cpr::GetAsync(cpr::Url {kBaseUrl_ + kListNwsTextProductsEndpoint_},
                       network::cpr::GetHeader(),
                       parameters,
                       network::cpr::GetDefaultTimeout(),
                       network::cpr::GetDefaultConnectTimeout(),
                       network::cpr::GetDefaultLowSpeed()));
   }

   return ProcessTextProductLists(asyncResponses);
}

template<ranges::forward_range Range>
   requires std::same_as<ranges::range_value_t<Range>, std::string>
std::vector<std::shared_ptr<awips::TextProductFile>>
IemApiProvider::LoadTextProducts(const Range& textProducts)
{
   auto parameters = cpr::Parameters {{"nolimit", "true"}};

   logger_->debug("Loading {} text products", textProducts.size());

   std::vector<std::pair<std::string, cpr::AsyncResponse>> asyncResponses {};
   asyncResponses.reserve(textProducts.size());

   const std::string endpointUrl = kBaseUrl_ + kNwsTextProductEndpoint_;

   for (const auto& productId : textProducts)
   {
      asyncResponses.emplace_back(
         productId,
         cpr::GetAsync(cpr::Url {endpointUrl + productId},
                       network::cpr::GetHeader(),
                       parameters,
                       network::cpr::GetDefaultTimeout(),
                       network::cpr::GetDefaultConnectTimeout(),
                       network::cpr::GetDefaultLowSpeed()));
   }

   return ProcessTextProductFiles(asyncResponses);
}

#ifdef kDateFormat
#   undef kDateFormat
#endif

} // namespace scwx::provider
