#include <scwx/provider/http_level3_data_provider.hpp>

#include <gtest/gtest.h>

namespace scwx::provider
{

static const char* kNwsLevel3BaseUri =
   "https://tgftp.nws.noaa.gov/SL.us008001/DF.of/DC.radar/";
static const char* kVolkronLevel3BaseUri =
   "http://sp2.volkron.net:8015/digatmos/nexrad/nids/";

TEST(HttpLevel3DataProviderTest, NwsGetFileUrlAfterDetection)
{
   HttpLevel3DataProvider provider("KLSX", "N0Q", kNwsLevel3BaseUri);

   provider.RequestAvailableProducts();

   const std::string url = provider.GetFileUrl("sn.0001");

   EXPECT_FALSE(url.empty());
   EXPECT_NE(url.find("DS.p94r0"), std::string::npos);
   EXPECT_NE(url.find("SI.klsx"), std::string::npos);
}

TEST(HttpLevel3DataProviderTest, NwsListObjects)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   HttpLevel3DataProvider provider("KLSX", "N0Q", kNwsLevel3BaseUri);

   const auto [success, newObjects, totalObjects] =
      provider.ListObjects(sys_days {floor<days>(system_clock::now())});

   if (!success)
   {
      GTEST_SKIP() << "Network unavailable or listing empty";
   }

   EXPECT_GT(totalObjects, 0u);
   EXPECT_GT(provider.cache_size(), 0u);
   EXPECT_EQ(newObjects, totalObjects);
}

TEST(HttpLevel3DataProviderTest, OndasListObjects)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   HttpLevel3DataProvider provider("KJAX", "N0Q", kVolkronLevel3BaseUri);

   const auto [success, newObjects, totalObjects] =
      provider.ListObjects(sys_days {floor<days>(system_clock::now())});

   if (!success)
   {
      GTEST_SKIP() << "Network unavailable or ONDAS listing empty";
   }

   EXPECT_GT(totalObjects, 0u);
   EXPECT_GT(provider.cache_size(), 0u);
}

TEST(HttpLevel3DataProviderTest, UnreachableServerListObjects)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   HttpLevel3DataProvider provider("KLSX", "N0Q", "http://127.0.0.1:1/");

   const auto [success, newObjects, totalObjects] =
      provider.ListObjects(sys_days {floor<days>(system_clock::now())});

   EXPECT_FALSE(success);
   EXPECT_EQ(newObjects, 0u);
   EXPECT_EQ(totalObjects, 0u);
   EXPECT_EQ(provider.GetFileUrl("sn.0001"), std::string {});
}

} // namespace scwx::provider
