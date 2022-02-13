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
   explicit NexradFileRequestImpl() : nexradFile_ {nullptr} {}

   ~NexradFileRequestImpl() {}

   std::shared_ptr<wsr88d::NexradFile> nexradFile_;
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

void NexradFileRequest::set_nexrad_file(
   std::shared_ptr<wsr88d::NexradFile> nexradFile)
{
   p->nexradFile_ = nexradFile;
}

} // namespace request
} // namespace qt
} // namespace scwx
