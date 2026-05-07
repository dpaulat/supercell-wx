#include <scwx/spc/spc_outlook_provider.hpp>
#include <scwx/spc/geojson_parser.hpp>
#include <scwx/network/cpr.hpp>
#include <scwx/util/logger.hpp>

#include <cpr/cpr.h>

#include <string>

namespace scwx::spc
{

static const std::string logPrefix_ = "scwx::spc::spc_outlook_provider";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

const std::string SpcOutlookProvider::kBaseUrl_ =
   "https://www.spc.noaa.gov/products/outlook/";

class SpcOutlookProvider::Impl
{
public:
   Impl()  = default;
   ~Impl() = default;
};

SpcOutlookProvider::SpcOutlookProvider() : p(std::make_unique<Impl>()) {}
SpcOutlookProvider::~SpcOutlookProvider()                             = default;
SpcOutlookProvider::SpcOutlookProvider(SpcOutlookProvider&&) noexcept = default;
SpcOutlookProvider&
SpcOutlookProvider::operator=(SpcOutlookProvider&&) noexcept = default;

std::string SpcOutlookProvider::GetOutlookUrl(OutlookDay     day,
                                              OutlookProduct product)
{
   std::string dayStr;
   switch (day)
   {
   case OutlookDay::Day1:
      dayStr = "day1otlk";
      break;
   case OutlookDay::Day2:
      dayStr = "day2otlk";
      break;
   case OutlookDay::Day3:
      dayStr = "day3otlk";
      break;
   default:
      return {};
   }

   std::string productStr;
   switch (product)
   {
   case OutlookProduct::Categorical:
      productStr = "cat";
      break;
   case OutlookProduct::Tornado:
      productStr = "torn";
      break;
   case OutlookProduct::Wind:
      productStr = "wind";
      break;
   case OutlookProduct::Hail:
      productStr = "hail";
      break;
   case OutlookProduct::Probabilistic:
      productStr = "prob";
      break;
   case OutlookProduct::SignificantProbabilistic:
      productStr = "cigprob";
      break;
   default:
      return {};
   }

   return kBaseUrl_ + dayStr + "_" + productStr + ".lyr.geojson";
}

boost::outcome_v2::result<OutlookData>
SpcOutlookProvider::FetchOutlook(OutlookDay day, OutlookProduct product)
{
   std::string url = GetOutlookUrl(day, product);
   if (url.empty())
   {
      return boost::outcome_v2::failure(
         std::make_error_code(std::errc::invalid_argument));
   }

   logger_->info("Fetching SPC outlook: {}", url);

   ::cpr::Response r = ::cpr::Get(::cpr::Url {url},
                                  network::cpr::GetHeader(),
                                  network::cpr::GetDefaultTimeout(),
                                  network::cpr::GetDefaultConnectTimeout(),
                                  network::cpr::GetDefaultLowSpeed());

   if (r.status_code != 200)
   {
      logger_->warn(
         "SPC outlook fetch failed: HTTP {} for URL {}", r.status_code, url);
      return boost::outcome_v2::failure(
         std::make_error_code(std::errc::io_error));
   }

   logger_->info("SPC outlook fetched successfully ({} bytes)", r.text.size());

   try
   {
      OutlookData data = ParseSpcGeoJson(day, product, r.text);
      logger_->info("Parsed {} polygons from SPC outlook",
                    data.polygons_.size());
      return data;
   }
   catch (const std::exception& ex)
   {
      logger_->warn("Failed to parse SPC outlook GeoJSON: {}", ex.what());
      return boost::outcome_v2::failure(
         std::make_error_code(std::errc::invalid_argument));
   }
}

} // namespace scwx::spc
