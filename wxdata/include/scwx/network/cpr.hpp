#pragma once

#include <scwx/common/application_state.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

#include <cpr/api.h>
#include <cpr/async_wrapper.h>
#include <cpr/cprtypes.h>

namespace scwx::network::cpr
{

using AsyncResponseC = ::cpr::AsyncWrapper<::cpr::Response, true>;

// Reports one download's incremental progress -- bytesReceived
// accumulates across every call for that download; totalBytes is
// cpr/libcurl's own reported total (from the response's Content-Length),
// or -1 if not known yet. Same shape/convention as
// provider::AwsNexradDataProvider::DownloadProgressCallback (a separate
// mechanism for S3 fetches specifically) -- this module has no
// dependency on wxdata's provider classes, so it gets its own,
// identically-shaped alias rather than sharing that one.
//
// Deliberately just a callback, not a direct dependency on any status-
// reporting singleton: this file lives in wxdata, which does not (and
// should not) link Qt/scwx-qt, so it cannot call scwx::qt::manager::
// StatusManager directly -- callers that live in scwx-qt (which does
// depend on wxdata, never the reverse) supply a callback that reports to
// it; wxdata-level callers that don't are unaffected (this parameter
// defaults to a no-op everywhere it was already optional).
using DownloadProgressCallback =
   std::function<void(std::int64_t bytesReceived, std::int64_t totalBytes)>;

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

// `progressCallback`, when supplied, is invoked from the same libcurl
// progress callback this already used solely for the isRunning
// cancellation check -- every existing caller (none of which passed a
// second argument before this was added) keeps working unchanged.
::cpr::ProgressCallback GetDefaultProgressCallback(
   const std::atomic<bool>&        isRunning,
   const DownloadProgressCallback& progressCallback = nullptr);
::cpr::Header GetHeader();
void          SetUserAgent(const std::string& userAgent);

/**
 * Download a file to a string.
 * @param url The URL to download from.
 * @param isRunning Whether the application is running.
 * @param progressCallback Optional incremental download progress report.
 * @return A pair containing the string and the status code.
 */
std::pair<std::string, long> DownloadToString(
   const std::string&       url,
   const std::atomic<bool>& isRunning = common::ApplicationState::IsRunning(),
   const DownloadProgressCallback& progressCallback = nullptr);

/**
 * Download a file to a stream.
 * @param url The URL to download from.
 * @param isRunning Whether the application is running.
 * @param progressCallback Optional incremental download progress report.
 * @return A pair containing the stream and the status code.
 */
std::pair<std::stringstream, long> DownloadToStream(
   const std::string&       url,
   const std::atomic<bool>& isRunning = common::ApplicationState::IsRunning(),
   const DownloadProgressCallback& progressCallback = nullptr);

} // namespace scwx::network::cpr
