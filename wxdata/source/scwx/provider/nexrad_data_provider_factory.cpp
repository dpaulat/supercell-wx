#include <scwx/provider/nexrad_data_provider_factory.hpp>
#include <scwx/provider/aws_level2_data_provider.hpp>

namespace scwx
{
namespace provider
{

static const std::string logPrefix_ =
   "scwx::provider::nexrad_data_provider_factory";

std::shared_ptr<NexradDataProvider>
NexradDataProviderFactory::CreateLevel2DataProvider(
   const std::string& radarSite)
{
   return std::make_unique<AwsLevel2DataProvider>(radarSite);
}

} // namespace provider
} // namespace scwx
