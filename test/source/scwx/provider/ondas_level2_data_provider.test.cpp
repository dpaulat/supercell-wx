#include <scwx/provider/ondas_level2_data_provider.hpp>

#include <gtest/gtest.h>

namespace scwx::provider
{

static const char* kIowaStateLevel2BaseUri =
   "https://mesonet-nexrad.agron.iastate.edu/level2/raw/";

TEST(OndasLevel2DataProviderTest, GetTimePointFromKey)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   constexpr auto expectedTime = sys_days {2026y / January / 31d} + 18h + 30min;

   EXPECT_EQ(OndasLevel2DataProvider::GetTimePointFromKey("20260131_1830"),
             expectedTime);
   EXPECT_EQ(OndasLevel2DataProvider::GetTimePointFromKey("KILN_20260131_1830"),
             expectedTime);
}

TEST(OndasLevel2DataProviderTest, GetTimePointFromKeyInvalid)
{
   constexpr std::chrono::system_clock::time_point expectedTime {};

   EXPECT_EQ(OndasLevel2DataProvider::GetTimePointFromKey("invalid"),
             expectedTime);
}

TEST(OndasLevel2DataProviderTest, ListObjects)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   OndasLevel2DataProvider provider("KLSX", kIowaStateLevel2BaseUri);

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

} // namespace scwx::provider
