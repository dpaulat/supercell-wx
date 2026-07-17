#include <scwx/provider/nexrad_data_provider.hpp>

namespace scwx
{
namespace provider
{

static const std::string logPrefix_ = "scwx::provider::nexrad_data_provider";

class NexradDataProvider::Impl
{
public:
   explicit Impl(std::string radarSite) : radarSite_(std::move(radarSite)) {}
   ~Impl() = default;

   Impl(const Impl&)            = delete;
   Impl& operator=(const Impl&) = delete;
   Impl(Impl&&)                 = delete;
   Impl& operator=(Impl&&)      = delete;

   const std::string radarSite_;
};

NexradDataProvider::NexradDataProvider(const std::string& radarSite) :
    p(std::make_unique<Impl>(radarSite))
{
}
NexradDataProvider::~NexradDataProvider() = default;

std::string NexradDataProvider::radar_site() const
{
   return p->radarSite_;
}

void NexradDataProvider::RequestAvailableProducts() {}

std::vector<std::string> NexradDataProvider::GetAvailableProducts()
{
   return {};
}

} // namespace provider
} // namespace scwx
