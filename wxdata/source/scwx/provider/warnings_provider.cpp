#include <scwx/provider/warnings_provider.hpp>
#include <scwx/network/dir_list.hpp>
#include <scwx/util/logger.hpp>

#include <ranges>
#include <shared_mutex>

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#define LIBXML_HTML_ENABLED
#include <cpr/cpr.h>
#include <libxml/HTMLparser.h>
#include <re2/re2.h>

#if !(defined(_MSC_VER) || defined(__clange__))
#   include <date/date.h>
#endif

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

namespace scwx
{
namespace provider
{

static const std::string logPrefix_ = "scwx::provider::warnings_provider";
static const auto        logger_    = util::Logger::Create(logPrefix_);

class WarningsProvider::Impl
{
public:
   struct FileInfoRecord
   {
      std::chrono::system_clock::time_point startTime_ {};
      std::chrono::system_clock::time_point lastModified_ {};
      size_t                                size_ {};
      bool                                  updated_ {};
   };

   typedef std::map<std::string, FileInfoRecord> WarningFileMap;

   explicit Impl(const std::string& baseUrl) :
       baseUrl_ {baseUrl}, files_ {}, filesMutex_ {}
   {
   }

   ~Impl() {}

   std::string baseUrl_;

   WarningFileMap    files_;
   std::shared_mutex filesMutex_;
};

WarningsProvider::WarningsProvider(const std::string& baseUrl) :
    p(std::make_unique<Impl>(baseUrl))
{
}
WarningsProvider::~WarningsProvider() = default;

WarningsProvider::WarningsProvider(WarningsProvider&&) noexcept = default;
WarningsProvider&
WarningsProvider::operator=(WarningsProvider&&) noexcept = default;

std::pair<size_t, size_t>
WarningsProvider::ListFiles(std::chrono::system_clock::time_point newerThan)
{
   using namespace std::chrono;

#if !(defined(_MSC_VER) || defined(__clang__))
   using namespace date;
#endif

   static constexpr LazyRE2 reWarningsFilename = {
      "warnings_[0-9]{8}_[0-9]{2}.txt"};
   static const std::string dateTimeFormat {"warnings_%Y%m%d_%H.txt"};

   logger_->trace("Listing files");

   size_t updatedObjects = 0;
   size_t totalObjects   = 0;

   // Perform a directory listing
   auto records = network::DirList(p->baseUrl_);

   // Sort records by filename
   std::sort(records.begin(),
             records.end(),
             [](auto& a, auto& b) { return a.filename_ < b.filename_; });

   // Filter warning records
   auto warningRecords =
      records |
      std::views::filter(
         [](auto& record)
         {
            return record.type_ == std::filesystem::file_type::regular &&
                   RE2::FullMatch(record.filename_, *reWarningsFilename);
         });

   std::unique_lock lock(p->filesMutex_);

   Impl::WarningFileMap warningFileMap;

   // Store records
   for (auto& record : warningRecords)
   {
      // Determine start time
      std::chrono::sys_time<hours> startTime;
      std::istringstream           ssFilename {record.filename_};

      ssFilename >> parse(dateTimeFormat, startTime);

      // If start time is valid
      if (!ssFilename.fail())
      {
         // Determine if the record should be marked updated
         bool updated = true;
         auto it      = p->files_.find(record.filename_);
         if (it != p->files_.cend())
         {
            auto& existingRecord = it->second;

            updated = existingRecord.updated_ ||
                      record.size_ != existingRecord.size_ ||
                      record.mtime_ != existingRecord.lastModified_;
         }

         // Update object counts, but only if newer than threshold
         if (newerThan < startTime)
         {
            if (updated)
            {
               ++updatedObjects;
            }
            ++totalObjects;
         }

         // Store record
         warningFileMap.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(record.filename_),
            std::forward_as_tuple(
               startTime, record.mtime_, record.size_, updated));
      }
   }

   p->files_ = std::move(warningFileMap);

   return std::make_pair(updatedObjects, totalObjects);
}

std::vector<std::shared_ptr<awips::TextProductFile>>
WarningsProvider::LoadUpdatedFiles(
   std::chrono::system_clock::time_point newerThan)
{
   logger_->debug("Loading updated files");

   std::vector<std::shared_ptr<awips::TextProductFile>> updatedFiles;

   std::vector<std::pair<std::string, cpr::AsyncResponse>> asyncResponses;

   std::unique_lock lock(p->filesMutex_);

   // For each warning file
   for (auto& record : p->files_)
   {
      // If file is updated, and time is later than the threshold
      if (record.second.updated_ && newerThan < record.second.startTime_)
      {
         // Retrieve warning file
         asyncResponses.emplace_back(
            record.first,
            cpr::GetAsync(cpr::Url {p->baseUrl_ + "/" + record.first}));

         // Clear updated flag
         record.second.updated_ = false;
      }
   }

   lock.unlock();

   // Wait for warning files to load
   for (auto& asyncResponse : asyncResponses)
   {
      cpr::Response response = asyncResponse.second.get();
      if (response.status_code == cpr::status::HTTP_OK)
      {
         logger_->debug("Loading file: {}", asyncResponse.first);

         // Load file
         std::shared_ptr<awips::TextProductFile> textProductFile {
            std::make_shared<awips::TextProductFile>()};
         std::istringstream responseBody {response.text};
         if (textProductFile->LoadData(responseBody))
         {
            updatedFiles.push_back(textProductFile);
         }
      }
   }

   return updatedFiles;
}

} // namespace provider
} // namespace scwx
