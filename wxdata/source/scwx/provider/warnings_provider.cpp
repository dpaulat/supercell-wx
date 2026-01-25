// Prevent redefinition of __cpp_lib_format
#if defined(_MSC_VER)
#   include <yvals_core.h>
#endif

// Enable chrono formatters
#ifndef __cpp_lib_format
// NOLINTNEXTLINE(bugprone-reserved-identifier, cppcoreguidelines-macro-usage)
#   define __cpp_lib_format 202110L
#endif

#define LIBXML_HTML_ENABLED

#include <scwx/provider/warnings_provider.hpp>
#include <scwx/network/cpr.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <atomic>
#include <mutex>

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#include <cpr/cpr.h>

#if (__cpp_lib_chrono < 201907L)
#   include <date/date.h>
#endif

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

namespace scwx::provider
{

static const std::string logPrefix_ = "scwx::provider::warnings_provider";
static const auto        logger_    = util::Logger::Create(logPrefix_);

class WarningsProvider::Impl
{
public:
   struct FileInfoRecord
   {
      FileInfoRecord(std::string contentLength, std::string lastModified) :
          contentLengthStr_ {std::move(contentLength)},
          lastModifiedStr_ {std::move(lastModified)}
      {
      }

      std::string contentLengthStr_ {};
      std::string lastModifiedStr_ {};
   };

   using WarningFileMap = std::map<std::string, FileInfoRecord>;

   explicit Impl(std::string baseUrl) :
       baseUrl_ {std::move(baseUrl)}, files_ {}, filesMutex_ {}
   {
   }

   ~Impl() { running_ = false; }
   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   bool UpdateFileRecord(const cpr::Response& response,
                         const std::string&   filename);

   std::atomic<bool> running_ {true};

   cpr::Header header_ {network::cpr::GetHeader()};

   std::string baseUrl_;

   WarningFileMap files_;
   std::mutex     filesMutex_;
};

WarningsProvider::WarningsProvider(const std::string& baseUrl) :
    p(std::make_unique<Impl>(baseUrl))
{
}
WarningsProvider::~WarningsProvider() = default;

WarningsProvider::WarningsProvider(WarningsProvider&&) noexcept = default;
WarningsProvider&
WarningsProvider::operator=(WarningsProvider&&) noexcept = default;

std::vector<std::shared_ptr<awips::TextProductFile>>
WarningsProvider::LoadUpdatedFiles(
   std::chrono::sys_time<std::chrono::hours> startTime)
{
   using namespace std::chrono;

#if (__cpp_lib_chrono >= 201907L)
   namespace df = std;

   static constexpr std::string_view kDateTimeFormat {
      "warnings_{:%Y%m%d_%H}.txt"};
#else
   using namespace date;
   namespace df = date;

#   define kDateTimeFormat "warnings_%Y%m%d_%H.txt"
#endif

   std::vector<std::pair<std::string, cpr::AsyncResponse>> asyncCallbacks;
   std::vector<std::shared_ptr<awips::TextProductFile>>    updatedFiles;

   const std::chrono::sys_time<std::chrono::hours> now =
      std::chrono::floor<std::chrono::hours>(util::time::now());
   std::chrono::sys_time<std::chrono::hours> currentHour =
      (startTime != std::chrono::sys_time<std::chrono::hours> {}) ?
         startTime :
         now - std::chrono::hours {1};

   logger_->trace("Querying files newer than: {}", util::TimeString(startTime));

   while (currentHour <= now)
   {
      const std::string filename = df::format(kDateTimeFormat, currentHour);
      const std::string url      = p->baseUrl_ + "/" + filename;

      logger_->trace("HEAD request for file: {}", filename);

      auto headResponse =
         cpr::Head(cpr::Url {url},
                   p->header_,
                   network::cpr::GetDefaultTimeout(),
                   network::cpr::GetDefaultConnectTimeout(),
                   network::cpr::GetDefaultLowSpeed(),
                   network::cpr::GetDefaultProgressCallback(p->running_));

      if (headResponse.status_code == cpr::status::HTTP_OK)
      {
         const bool updated = p->UpdateFileRecord(headResponse, filename);

         if (updated)
         {
            logger_->trace("GET request for file: {}", filename);

            asyncCallbacks.emplace_back(
               filename,
               cpr::GetAsync(
                  cpr::Url {url},
                  p->header_,
                  network::cpr::GetDefaultTimeout(),
                  network::cpr::GetDefaultConnectTimeout(),
                  network::cpr::GetDefaultLowSpeed(),
                  network::cpr::GetDefaultProgressCallback(p->running_)));
         }
      }
      else if (headResponse.status_code != cpr::status::HTTP_NOT_FOUND &&
               p->running_)
      {
         logger_->warn("HEAD request for file failed: {} ({})",
                       url,
                       headResponse.status_line);
      }

      // Query the next hour
      currentHour += 1h;
   }

   for (auto& asyncCallback : asyncCallbacks)
   {
      auto& filename      = asyncCallback.first;
      auto& asyncResponse = asyncCallback.second;

      if (asyncResponse.valid())
      {
         // Wait for futures to complete
         asyncResponse.wait();
         auto response = asyncResponse.get();

         if (response.status_code == cpr::status::HTTP_OK)
         {
            logger_->debug("Loading file: {}", filename);

            // Load file
            const std::shared_ptr<awips::TextProductFile> textProductFile {
               std::make_shared<awips::TextProductFile>()};
            std::istringstream responseBody {response.text};
            if (textProductFile->LoadData(filename, responseBody))
            {
               updatedFiles.push_back(textProductFile);
            }
         }
         else
         {
            logger_->warn(
               "Could not load file: {} ({})", filename, response.status_line);
         }
      }
      else
      {
         logger_->error("Invalid future state");
      }
   }

   return updatedFiles;
}

bool WarningsProvider::Impl::UpdateFileRecord(const cpr::Response& response,
                                              const std::string&   filename)
{
   bool updated = false;

   auto contentLengthIt = response.header.find("Content-Length");
   auto lastModifiedIt  = response.header.find("Last-Modified");

   std::string contentLength {};
   std::string lastModified {};

   if (contentLengthIt != response.header.cend())
   {
      contentLength = contentLengthIt->second;
   }
   if (lastModifiedIt != response.header.cend())
   {
      lastModified = lastModifiedIt->second;
   }

   const std::unique_lock lock(filesMutex_);

   auto it = files_.find(filename);
   if (it != files_.cend())
   {
      auto& existingRecord = it->second;

      // If the size or last modified changes, request an update

      if (!contentLength.empty() &&
          contentLength != existingRecord.contentLengthStr_)
      {
         // Size changed
         existingRecord.contentLengthStr_ = contentLengthIt->second;
         updated                          = true;
      }
      else if (!lastModified.empty() &&
               lastModified != existingRecord.lastModifiedStr_)
      {
         // Last modified changed
         existingRecord.lastModifiedStr_ = lastModifiedIt->second;
         updated                         = true;
      }
   }
   else
   {
      // File not found
      files_.emplace(std::piecewise_construct,
                     std::forward_as_tuple(filename),
                     std::forward_as_tuple(contentLength, lastModified));
      updated = true;
   }

   return updated;
}

void WarningsProvider::Shutdown() noexcept
{
   p->running_ = false;
}

} // namespace scwx::provider
