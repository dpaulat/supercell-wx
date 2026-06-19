#include <scwx/provider/nws_api_provider.hpp>

#include <gtest/gtest.h>

namespace scwx::provider
{

TEST(NwsApiProviderTest, GetRadarStations)
{
   NwsApiProvider provider {};
   auto           radarStations = provider.GetRadarStations();
   EXPECT_EQ(radarStations.has_value(), true);
}

} // namespace scwx::provider
