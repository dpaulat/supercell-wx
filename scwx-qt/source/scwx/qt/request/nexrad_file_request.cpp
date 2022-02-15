#include <scwx/qt/request/nexrad_file_request.hpp>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace qt
{
namespace request
{

static const std::string logPrefix_ =
   "[scwx::qt::request::nexrad_file_request] ";

class NexradFileRequestImpl
{
public:
   explicit NexradFileRequestImpl() :
       nexradFile_ {nullptr}, radarId_ {}, siteId_ {}, time_ {}
   {
   }

   ~NexradFileRequestImpl() {}

   std::shared_ptr<wsr88d::NexradFile>   nexradFile_;
   std::string                           radarId_;
   std::string                           siteId_;
   std::chrono::system_clock::time_point time_;
};

NexradFileRequest::NexradFileRequest() :
    p(std::make_unique<NexradFileRequestImpl>())
{
}
NexradFileRequest::~NexradFileRequest() = default;

std::shared_ptr<wsr88d::NexradFile> NexradFileRequest::nexrad_file() const
{
   return p->nexradFile_;
}

std::string NexradFileRequest::radar_id() const
{
   return p->radarId_;
}

std::string NexradFileRequest::site_id() const
{
   return p->siteId_;
}

std::chrono::system_clock::time_point NexradFileRequest::time() const
{
   return p->time_;
}

void NexradFileRequest::set_nexrad_file(
   std::shared_ptr<wsr88d::NexradFile> nexradFile)
{
   p->nexradFile_ = nexradFile;
}

void NexradFileRequest::set_radar_id(const std::string& radarId)
{
   p->radarId_ = radarId;
}

void NexradFileRequest::set_site_id(const std::string& siteId)
{
   p->siteId_ = siteId;
}

void NexradFileRequest::set_time(std::chrono::system_clock::time_point time)
{
   p->time_ = time;
}

} // namespace request
} // namespace qt
} // namespace scwx
