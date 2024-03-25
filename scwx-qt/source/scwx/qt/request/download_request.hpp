#pragma once

#include <filesystem>
#include <memory>

#include <QObject>

namespace scwx
{
namespace qt
{
namespace request
{

class DownloadRequest : public QObject
{
   Q_OBJECT

public:
   enum class CompleteReason
   {
      OK,
      Canceled,
      IOError,
      RemoteError
   };

   explicit DownloadRequest(const std::string&           url,
                            const std::filesystem::path& destinationPath);
   ~DownloadRequest();

   const std::string&           url() const;
   const std::filesystem::path& destination_path() const;

   void Cancel();

   bool IsCanceled() const;

private:
   class Impl;
   std::unique_ptr<Impl> p;

signals:
   void ProgressUpdated(std::ptrdiff_t downloadedBytes,
                        std::ptrdiff_t totalBytes);
   void RequestComplete(CompleteReason reason);
};

} // namespace request
} // namespace qt
} // namespace scwx
