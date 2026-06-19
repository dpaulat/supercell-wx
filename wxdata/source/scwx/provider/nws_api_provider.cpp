#include <scwx/provider/nws_api_provider.hpp>
#include <scwx/network/cpr.hpp>
#include <scwx/util/json.hpp>
#include <scwx/util/logger.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/json.hpp>
#include <cpr/cpr.h>

namespace scwx::provider
{

static const std::string logPrefix_ = "scwx::provider::nws_api_provider";
static const auto        logger_    = util::Logger::Create(logPrefix_);

const std::string kBaseUrl_ = "https://api.weather.gov";

const std::string kRadarStationsEndpoint_ = "/radar/stations";

class NwsApiProvider::Impl
{
public:
   explicit Impl()
   {
      // Request JSON-LD instead of GeoJSON
      header_.insert({"accept", "application/ld+json"});
   }
   ~Impl() { running_ = false; }
   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   template<typename T>
   boost::outcome_v2::result<T> RequestData(std::string_view       endpointUrl,
                                            const cpr::Parameters& parameters);

   cpr::Header header_ {network::cpr::GetHeader()};

   std::atomic<bool> running_ {true};
};

NwsApiProvider::NwsApiProvider() : p(std::make_unique<Impl>()) {}
NwsApiProvider::~NwsApiProvider() = default;

NwsApiProvider::NwsApiProvider(NwsApiProvider&&) noexcept            = default;
NwsApiProvider& NwsApiProvider::operator=(NwsApiProvider&&) noexcept = default;

boost::outcome_v2::result<std::vector<types::nws::ObservationStation>>
NwsApiProvider::GetRadarStations(const std::vector<std::string>& stationType,
                                 std::optional<std::string_view> reportingHost,
                                 std::optional<std::string_view> host)
{
   logger_->debug("GetRadarStations");

   static const std::string kEndpointUrl = kBaseUrl_ + kRadarStationsEndpoint_;

   auto parameters = cpr::Parameters {};

   if (!stationType.empty())
   {
      parameters.Add({"stationType", boost::algorithm::join(stationType, ",")});
   }
   if (reportingHost.has_value())
   {
      parameters.Add({"reportingHost", std::string {*reportingHost}});
   }
   if (host.has_value())
   {
      parameters.Add({"host", std::string {*host}});
   }

   auto stationCollection =
      p->RequestData<types::nws::ObservationStationCollection>(kEndpointUrl,
                                                               parameters);

   if (stationCollection.has_value())
   {
      return stationCollection.value().graph_;
   }
   else
   {
      return stationCollection.error();
   }
}

template<typename T>
boost::outcome_v2::result<T>
NwsApiProvider::Impl::RequestData(std::string_view       endpointUrl,
                                  const cpr::Parameters& parameters)
{
   T data;

   auto asyncResponse =
      cpr::GetAsync(cpr::Url {endpointUrl},
                    header_,
                    parameters,
                    network::cpr::GetDefaultTimeout(),
                    network::cpr::GetDefaultConnectTimeout(),
                    network::cpr::GetDefaultLowSpeed(),
                    network::cpr::GetDefaultProgressCallback(running_));

   const auto response = asyncResponse.get();

   const boost::json::value json = util::json::ReadJsonString(response.text);

   if (response.status_code == cpr::status::HTTP_OK)
   {
      try
      {
         data = boost::json::value_to<T>(json);
      }
      catch (const std::exception& ex)
      {
         // Unexpected bad response
         logger_->warn("Error parsing JSON: {}", ex.what());
         return boost::system::errc::make_error_code(
            boost::system::errc::bad_message);
      }
   }
   else if (json != nullptr)
   {
      try
      {
         // Log error response details
         const auto errorResponse =
            boost::json::value_to<types::nws::ErrorResponse>(json);
         logger_->warn("GetRadarStations error response ({}): {} ({})",
                       response.status_code,
                       errorResponse.title_,
                       errorResponse.detail_);

         for (auto& parameterError : errorResponse.parameterErrors_)
         {
            logger_->warn(" Parameter error: {} {}",
                          parameterError.parameter_,
                          parameterError.message_);
         }

         return boost::system::errc::make_error_code(
            boost::system::errc::no_message);
      }
      catch (const std::exception& ex)
      {
         // Unexpected bad response
         logger_->warn("Error parsing error response ({}): {}",
                       response.status_code,
                       ex.what());

         return boost::system::errc::make_error_code(
            boost::system::errc::no_message);
      }
   }
   else if (running_)
   {
      logger_->warn("Could not get radar stations: {}", response.status_code);

      return boost::system::errc::make_error_code(
         boost::system::errc::no_message);
   }

   return data;
}

void NwsApiProvider::Shutdown() noexcept
{
   p->running_ = false;
}

} // namespace scwx::provider
