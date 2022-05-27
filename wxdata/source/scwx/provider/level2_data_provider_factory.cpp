#include <scwx/wsr88d/rda/level2_message_factory.hpp>

#include <scwx/util/logger.hpp>
#include <scwx/util/vectorbuf.hpp>
#include <scwx/wsr88d/rda/clutter_filter_bypass_map.hpp>
#include <scwx/wsr88d/rda/clutter_filter_map.hpp>
#include <scwx/wsr88d/rda/digital_radar_data.hpp>
#include <scwx/wsr88d/rda/performance_maintenance_data.hpp>
#include <scwx/wsr88d/rda/rda_adaptation_data.hpp>
#include <scwx/provider/level2_data_provider_factory.hpp>
#include <scwx/provider/aws_level2_data_provider.hpp>

namespace scwx
{
namespace provider
{

static const std::string logPrefix_ =
   "scwx::provider::level2_data_provider_factory";

std::shared_ptr<Level2DataProvider>
Level2DataProviderFactory::Create(const std::string& radarSite)
{
   return std::make_unique<AwsLevel2DataProvider>(radarSite);
}

} // namespace provider
} // namespace scwx
