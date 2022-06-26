#include <scwx/provider/nexrad_data_provider.hpp>

namespace scwx
{
namespace provider
{

static const std::string logPrefix_ = "scwx::provider::nexrad_data_provider";

class NexradDataProvider::Impl
{
public:
   explicit Impl() {}

   ~Impl() {}
};

NexradDataProvider::NexradDataProvider() : p(std::make_unique<Impl>()) {}
NexradDataProvider::~NexradDataProvider() = default;

NexradDataProvider::NexradDataProvider(NexradDataProvider&&) noexcept = default;
NexradDataProvider&
NexradDataProvider::operator=(NexradDataProvider&&) noexcept = default;

void NexradDataProvider::RequestAvailableProducts() {}

std::vector<std::string> NexradDataProvider::GetAvailableProducts()
{
   return {};
}

} // namespace provider
} // namespace scwx
