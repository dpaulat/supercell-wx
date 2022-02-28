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
   explicit NexradFileRequestImpl() : radarProductRecord_ {nullptr} {}

   ~NexradFileRequestImpl() {}

   std::shared_ptr<types::RadarProductRecord> radarProductRecord_;
};

NexradFileRequest::NexradFileRequest() :
    p(std::make_unique<NexradFileRequestImpl>())
{
}
NexradFileRequest::~NexradFileRequest() = default;

std::shared_ptr<types::RadarProductRecord>
NexradFileRequest::radar_product_record() const
{
   return p->radarProductRecord_;
}

void NexradFileRequest::set_radar_product_record(
   std::shared_ptr<types::RadarProductRecord> record)
{
   p->radarProductRecord_ = record;
}

} // namespace request
} // namespace qt
} // namespace scwx
