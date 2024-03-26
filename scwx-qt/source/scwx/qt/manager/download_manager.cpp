#include <scwx/qt/manager/download_manager.hpp>
#include <scwx/util/digest.hpp>
#include <scwx/util/logger.hpp>

#include <fstream>

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <cpr/cpr.h>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::download_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class DownloadManager::Impl
{
public:
   explicit Impl(DownloadManager* self) : self_ {self} {}

   ~Impl() { threadPool_.join(); }

   boost::asio::thread_pool threadPool_ {1u};

   DownloadManager* self_;
};

DownloadManager::DownloadManager() : p(std::make_unique<Impl>(this)) {}
DownloadManager::~DownloadManager() = default;

void DownloadManager::Download(
   const std::shared_ptr<request::DownloadRequest>& request)
{
   boost::asio::post(
      p->threadPool_,
      [=]()
      {
         // Prepare destination file
         const std::filesystem::path& destinationPath =
            request->destination_path();

         if (!destinationPath.has_parent_path())
         {
            logger_->error("Destination has no parent path: \"{}\"");

            Q_EMIT request->RequestComplete(
               request::DownloadRequest::CompleteReason::IOError);

            return;
         }

         const std::filesystem::path parentPath = destinationPath.parent_path();

         // Create directory if it doesn't exist
         if (!std::filesystem::exists(parentPath))
         {
            if (!std::filesystem::create_directories(parentPath))
            {
               logger_->error("Unable to create download directory: \"{}\"",
                              parentPath.string());

               Q_EMIT request->RequestComplete(
                  request::DownloadRequest::CompleteReason::IOError);

               return;
            }
         }

         // Remove file if it exists
         if (std::filesystem::exists(destinationPath))
         {
            std::error_code error;
            if (!std::filesystem::remove(destinationPath, error))
            {
               logger_->error(
                  "Unable to remove existing destination file ({}): \"{}\"",
                  error.message(),
                  destinationPath.string());

               Q_EMIT request->RequestComplete(
                  request::DownloadRequest::CompleteReason::IOError);

               return;
            }
         }

         // Open file for writing
         std::ofstream ofs {destinationPath,
                            std::ios_base::out | std::ios_base::binary |
                               std::ios_base::trunc};
         if (!ofs.is_open() || !ofs.good())
         {
            logger_->error(
               "Unable to open destination file for writing: \"{}\"",
               destinationPath.string());

            Q_EMIT request->RequestComplete(
               request::DownloadRequest::CompleteReason::IOError);

            return;
         }

         // Download file
         cpr::Response response = cpr::Get(
            cpr::Url {request->url()},
            cpr::ProgressCallback(
               [=](cpr::cpr_off_t downloadTotal,
                   cpr::cpr_off_t downloadNow,
                   cpr::cpr_off_t /* uploadTotal */,
                   cpr::cpr_off_t /* uploadNow */,
                   std::intptr_t /* userdata */)
               {
                  Q_EMIT request->ProgressUpdated(downloadNow, downloadTotal);
                  return !request->IsCanceled();
               }),
            cpr::WriteCallback(
               [=, &ofs](std::string data, std::intptr_t /* userdata */)
               {
                  // Write file
                  ofs << data;
                  return !request->IsCanceled();
               }));

         bool ofsGood = ofs.good();
         ofs.close();

         // Handle error response
         if (response.error.code != cpr::ErrorCode::OK ||
             request->IsCanceled() || !ofsGood)
         {
            request::DownloadRequest::CompleteReason reason =
               request::DownloadRequest::CompleteReason::IOError;

            if (request->IsCanceled())
            {
               logger_->info("Download request cancelled: \"{}\"",
                             request->url());

               reason = request::DownloadRequest::CompleteReason::Canceled;
            }
            else if (response.error.code != cpr::ErrorCode::OK)
            {
               logger_->error("Error downloading file ({}): \"{}\"",
                              response.error.message,
                              request->url());

               reason = request::DownloadRequest::CompleteReason::RemoteError;
            }
            else if (!ofsGood)
            {
               logger_->error("File I/O error: \"{}\"",
                              destinationPath.string());

               reason = request::DownloadRequest::CompleteReason::IOError;
            }

            std::error_code error;
            if (!std::filesystem::remove(destinationPath, error))
            {
               logger_->error("Unable to remove destination file: \"{}\", {}",
                              destinationPath.string(),
                              error.message());
            }

            Q_EMIT request->RequestComplete(reason);

            return;
         }

         // Handle response
         const auto contentMd5 = response.header.find("content-md5");
         if (contentMd5 != response.header.cend() &&
             !contentMd5->second.empty())
         {
            // Open file for reading
            std::ifstream is {destinationPath,
                              std::ios_base::in | std::ios_base::binary};
            if (!is.is_open() || !is.good())
            {
               logger_->error(
                  "Unable to open destination file for reading: \"{}\"",
                  destinationPath.string());

               Q_EMIT request->RequestComplete(
                  request::DownloadRequest::CompleteReason::IOError);

               return;
            }

            // Compute MD5
            std::vector<std::uint8_t> digest {};
            if (!util::ComputeDigest(EVP_md5(), is, digest))
            {
               logger_->error("Failed to compute MD5: \"{}\"",
                              destinationPath.string());

               Q_EMIT request->RequestComplete(
                  request::DownloadRequest::CompleteReason::IOError);

               return;
            }

            // Compare calculated MD5 with digest in response header
            QByteArray expectedDigestArray =
               QByteArray::fromBase64(contentMd5->second.c_str());
            std::vector<std::uint8_t> expectedDigest(
               expectedDigestArray.cbegin(), expectedDigestArray.cend());

            if (digest != expectedDigest)
            {
               QByteArray calculatedDigest(
                  reinterpret_cast<char*>(digest.data()), digest.size());

               logger_->error("Digest mismatch: {} != {}",
                              calculatedDigest.toBase64().toStdString(),
                              contentMd5->second);

               Q_EMIT request->RequestComplete(
                  request::DownloadRequest::CompleteReason::DigestError);

               return;
            }
         }

         logger_->info("Download complete: \"{}\"", request->url());
         Q_EMIT request->RequestComplete(
            request::DownloadRequest::CompleteReason::OK);
      });
}

std::shared_ptr<DownloadManager> DownloadManager::Instance()
{
   static std::weak_ptr<DownloadManager> downloadManagerReference_ {};
   static std::mutex                     instanceMutex_ {};

   std::unique_lock lock(instanceMutex_);

   std::shared_ptr<DownloadManager> downloadManager =
      downloadManagerReference_.lock();

   if (downloadManager == nullptr)
   {
      downloadManager           = std::make_shared<DownloadManager>();
      downloadManagerReference_ = downloadManager;
   }

   return downloadManager;
}

} // namespace manager
} // namespace qt
} // namespace scwx
