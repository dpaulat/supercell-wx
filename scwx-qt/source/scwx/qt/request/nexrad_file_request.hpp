#pragma once

#include <scwx/wsr88d/nexrad_file.hpp>

#include <chrono>
#include <memory>

#include <QObject>

namespace scwx
{
namespace qt
{
namespace request
{

class NexradFileRequestImpl;

class NexradFileRequest : public QObject
{
   Q_OBJECT

public:
   explicit NexradFileRequest();
   ~NexradFileRequest();

   std::shared_ptr<wsr88d::NexradFile>   nexrad_file() const;
   std::string                           radar_id() const;
   std::string                           site_id() const;
   std::chrono::system_clock::time_point time() const;

   void set_nexrad_file(std::shared_ptr<wsr88d::NexradFile> nexradFile);
   void set_radar_id(const std::string& radarId);
   void set_site_id(const std::string& siteId);
   void set_time(std::chrono::system_clock::time_point time);

private:
   std::unique_ptr<NexradFileRequestImpl> p;

signals:
   void RequestComplete(std::shared_ptr<NexradFileRequest> request);
};

} // namespace request
} // namespace qt
} // namespace scwx
