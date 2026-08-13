// Enable chrono formatter feature test macro
#include <version>

#define LIBXML_HTML_ENABLED

#include <scwx/provider/warnings_provider.hpp>
#include <scwx/network/cpr.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <atomic>
#include <mutex>
#include <optional>
#include <vector>

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
   const std::chrono::sys_time<std::chrono::hours>                startTime,
   const std::optional<std::chrono::sys_time<std::chrono::hours>> endBefore)
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

   std::vector<
      std::pair<std::string,
                cpr::AsyncWrapper<std::optional<cpr::AsyncResponse>, false>>>
                                                        asyncCallbacks;
   std::vector<std::shared_ptr<awips::TextProductFile>> updatedFiles;

   const std::chrono::sys_time<std::chrono::hours> now =
      std::chrono::floor<std::chrono::hours>(util::time::now());
   const std::chrono::sys_time<std::chrono::hours> rangeStart =
      (startTime != std::chrono::sys_time<std::chrono::hours> {}) ?
         startTime :
         now - std::chrono::hours {1};

   const std::chrono::sys_time<std::chrono::hours> rangeEnd =
      endBefore.has_value() ? std::min(now, *endBefore - 1h) : now;

   if (rangeEnd < rangeStart)
   {
      return updatedFiles;
   }

   logger_->trace("Querying warning files from {} through {}",
                  util::TimeString(rangeStart),
                  util::TimeString(rangeEnd));

   for (std::chrono::sys_time<std::chrono::hours> currentHour = rangeEnd;
        currentHour >= rangeStart;
        currentHour -= 1h)
   {
      const std::string filename = df::format(kDateTimeFormat, currentHour);
      const std::string url      = p->baseUrl_ + "/" + filename;

      logger_->trace("HEAD request for file: {}", filename);

      asyncCallbacks.emplace_back(
         filename,
         cpr::HeadCallback(
            [url, filename, this](
               cpr::Response headResponse) -> std::optional<cpr::AsyncResponse>
            {
               if (headResponse.status_code == cpr::status::HTTP_OK)
               {
                  const bool updated =
                     p->UpdateFileRecord(headResponse, filename);

                  if (updated)
                  {
                     logger_->trace("GET request for file: {}", filename);
                     return cpr::GetAsync(
                        cpr::Url {url},
                        network::cpr::GetHeader(),
                        network::cpr::GetDefaultTimeout(),
                        network::cpr::GetDefaultConnectTimeout(),
                        network::cpr::GetDefaultLowSpeed(),
                        network::cpr::GetDefaultProgressCallback(p->running_));
                  }
               }
               else if (headResponse.status_code != cpr::status::HTTP_NOT_FOUND)
               {
                  if (p->running_)
                  {
                     logger_->warn("HEAD request for file failed: {} ({})",
                                   url,
                                   headResponse.status_line);
                  }
                  else
                  {
                     logger_->debug("HEAD request for file cancelled: {}",
                                    filename);
                  }
               }

               return std::nullopt;
            },
            cpr::Url {url},
            network::cpr::GetHeader(),
            network::cpr::GetDefaultTimeout(),
            network::cpr::GetDefaultConnectTimeout(),
            network::cpr::GetDefaultLowSpeed(),
            network::cpr::GetDefaultProgressCallback(p->running_)));
   }

   for (auto& asyncCallback : asyncCallbacks)
   {
      auto& filename = asyncCallback.first;
      auto& callback = asyncCallback.second;

      if (callback.valid())
      {
         // Wait for futures to complete
         callback.wait();
         auto asyncResponse = callback.get();

         if (asyncResponse.has_value())
         {
            auto response = asyncResponse.value().get();

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
            else if (p->running_)
            {
               logger_->warn("Could not load file: {} ({})",
                             filename,
                             response.status_line);
            }
            else
            {
               logger_->debug("Request for file cancelled: {}", filename);
            }
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
