#include <scwx/network/cpr.hpp>
#include <scwx/util/logger.hpp>

#include <chrono>

#include <cpr/status_codes.h>

namespace scwx::network::cpr
{

static const std::string logPrefix_ = "scwx::network::cpr";
static const auto        logger_    = util::Logger::Create(logPrefix_);

using namespace std::chrono_literals;

static constexpr std::chrono::seconds kConnectTimeout_ = 10s;
static constexpr std::chrono::seconds kTimeout_        = 30s;
static constexpr std::int32_t         kLowSpeedLimit_  = 512; // bytes/sec
static constexpr std::chrono::seconds kLowSpeedTime_   = 15s;

static ::cpr::Header header_ {};

::cpr::ConnectTimeout GetDefaultConnectTimeout()
{
   return ::cpr::ConnectTimeout {kConnectTimeout_};
}

::cpr::Timeout GetDefaultTimeout()
{
   return ::cpr::Timeout {kTimeout_};
}

::cpr::LowSpeed GetDefaultLowSpeed()
{
   return ::cpr::LowSpeed {kLowSpeedLimit_, kLowSpeedTime_};
}

::cpr::ProgressCallback
GetDefaultProgressCallback(const std::atomic<bool>& isRunning)
{
   return ::cpr::ProgressCallback([&](::cpr::cpr_off_t /* downloadTotal */,
                                      ::cpr::cpr_off_t /* downloadNow */,
                                      ::cpr::cpr_off_t /* uploadTotal */,
                                      ::cpr::cpr_off_t /* uploadNow */,
                                      std::intptr_t /* userdata */)
                                  { return isRunning.load(); });
}

::cpr::Header GetHeader()
{
   return header_;
}

void SetUserAgent(const std::string& userAgent)
{
   header_.insert_or_assign("User-Agent", userAgent);
}

std::string DownloadToString(const std::string&       url,
                             const std::atomic<bool>& isRunning)
{
   // Use CPR to download file
   ::cpr::Response response =
      ::cpr::Get(::cpr::Url {url},
                 network::cpr::GetHeader(),
                 network::cpr::GetDefaultTimeout(),
                 network::cpr::GetDefaultConnectTimeout(),
                 network::cpr::GetDefaultLowSpeed(),
                 GetDefaultProgressCallback(isRunning));

   if (response.status_code != ::cpr::status::HTTP_OK)
   {
      logger_->warn("Failed to download {}: {} ({})",
                    url,
                    response.error.message,
                    response.status_code);
      return {};
   }

   return response.text;
}

std::stringstream DownloadToStream(const std::string&       url,
                                   const std::atomic<bool>& isRunning)
{
   // Convert response to stream
   std::stringstream ss {DownloadToString(url, isRunning),
                         std::ios::in | std::ios::binary};
   return ss;
}

} // namespace scwx::network::cpr
