#pragma once

#include <scwx/common/application_state.hpp>

#include <atomic>
#include <memory>

#include <cpr/api.h>
#include <cpr/async_wrapper.h>
#include <cpr/cprtypes.h>

namespace scwx::network::cpr
{

using AsyncResponseC = ::cpr::AsyncWrapper<::cpr::Response, true>;

template<typename... Ts>
std::shared_ptr<AsyncResponseC> GetAsyncC(Ts... ts)
{
   std::vector<AsyncResponseC> responses =
      ::cpr::MultiGetAsync(std::tuple {std::forward<Ts>(ts)...});
   auto responsePtr = std::make_shared<AsyncResponseC>(std::move(responses[0]));
   return responsePtr;
}

::cpr::ConnectTimeout GetDefaultConnectTimeout();
::cpr::Timeout        GetDefaultTimeout();
::cpr::LowSpeed       GetDefaultLowSpeed();
::cpr::ProgressCallback
              GetDefaultProgressCallback(const std::atomic<bool>& isRunning);
::cpr::Header GetHeader();
void          SetUserAgent(const std::string& userAgent);

std::string DownloadToString(
   const std::string&       url,
   const std::atomic<bool>& isRunning = common::ApplicationState::IsRunning());
std::stringstream DownloadToStream(
   const std::string&       url,
   const std::atomic<bool>& isRunning = common::ApplicationState::IsRunning());

} // namespace scwx::network::cpr
