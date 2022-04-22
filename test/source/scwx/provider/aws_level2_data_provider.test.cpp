#include <scwx/provider/aws_level2_data_provider.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace provider
{

TEST(AwsLevel2DataProvider, LoadObjectByKey)
{
   const std::string key = "2022/04/21/KLSX/KLSX20220421_160055_V06";

   AwsLevel2DataProvider provider("KLSX");

   auto file = provider.LoadObjectByKey(key);

   EXPECT_NE(file, nullptr);
}

TEST(AwsLevel2DataProvider, Refresh)
{
   AwsLevel2DataProvider provider("KLSX");

   provider.Refresh();

   // TODO: Check object count
}

} // namespace provider
} // namespace scwx
