#include <scwx/qt/request/download_request.hpp>

namespace scwx
{
namespace qt
{
namespace request
{

static const std::string logPrefix_ = "scwx::qt::request::download_request";

class DownloadRequest::Impl
{
public:
   explicit Impl(const std::string&           url,
                 const std::filesystem::path& destinationPath) :
       url_ {url}, destinationPath_ {destinationPath}
   {
   }
   ~Impl() = default;

   const std::string           url_;
   const std::filesystem::path destinationPath_;

   bool canceled_ = false;
};

DownloadRequest::DownloadRequest(const std::string&           url,
                                 const std::filesystem::path& destinationPath) :
    p(std::make_unique<Impl>(url, destinationPath))
{
}
DownloadRequest::~DownloadRequest() = default;

const std::string& DownloadRequest::url() const
{
   return p->url_;
}

const std::filesystem::path& DownloadRequest::destination_path() const
{
   return p->destinationPath_;
}

void DownloadRequest::Cancel()
{
   p->canceled_ = true;
}

bool DownloadRequest::IsCanceled() const
{
   return p->canceled_;
}

} // namespace request
} // namespace qt
} // namespace scwx
