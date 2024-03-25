#pragma once

#include <scwx/qt/request/download_request.hpp>

#include <memory>
#include <string>

#include <QObject>

namespace scwx
{
namespace qt
{
namespace manager
{

class DownloadManager : public QObject
{
   Q_OBJECT

public:
   explicit DownloadManager();
   ~DownloadManager();

   void Download(const std::shared_ptr<request::DownloadRequest>& request);

   static std::shared_ptr<DownloadManager> Instance();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
