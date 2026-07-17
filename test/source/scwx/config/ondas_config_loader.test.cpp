#include <scwx/config/ondas_config_loader.hpp>

#include <gtest/gtest.h>

namespace scwx::config
{

static const char* kNwsLevel3BaseUri =
   "https://tgftp.nws.noaa.gov/SL.us008001/DF.of/DC.radar/";
static const char* kIowaStateLevel2BaseUri =
   "https://mesonet-nexrad.agron.iastate.edu/level2/raw/";
static const char* kUnreachableBaseUri = "http://127.0.0.1:1/";

TEST(OndasConfigLoaderTest, FetchNwsNotFound)
{
   const auto result = OndasConfigLoader::Fetch(kNwsLevel3BaseUri);

   if (result.status == OndasConfigLoader::Status::Error)
   {
      GTEST_SKIP() << "Network unavailable";
   }

   EXPECT_EQ(result.status, OndasConfigLoader::Status::NotFound);
   EXPECT_EQ(result.config, nullptr);
}

TEST(OndasConfigLoaderTest, GetCachesNotFound)
{
   const auto result1 = OndasConfigLoader::Get(kNwsLevel3BaseUri);

   if (result1.status == OndasConfigLoader::Status::Error)
   {
      GTEST_SKIP() << "Network unavailable";
   }

   const auto result2 = OndasConfigLoader::Get(kNwsLevel3BaseUri);

   EXPECT_EQ(result1.status, OndasConfigLoader::Status::NotFound);
   EXPECT_EQ(result2.status, OndasConfigLoader::Status::NotFound);
   EXPECT_EQ(result1.config, nullptr);
   EXPECT_EQ(result2.config, nullptr);
}

TEST(OndasConfigLoaderTest, GetIowaStateLoaded)
{
   const auto result = OndasConfigLoader::Get(kIowaStateLevel2BaseUri);

   if (result.status != OndasConfigLoader::Status::Loaded)
   {
      GTEST_SKIP() << "Network unavailable or ONDAS config not found";
   }

   ASSERT_NE(result.config, nullptr);
   EXPECT_FALSE(result.config->list_file().empty());
}

TEST(OndasConfigLoaderTest, GetDoesNotCacheError)
{
   const auto result1 = OndasConfigLoader::Get(kUnreachableBaseUri);
   const auto result2 = OndasConfigLoader::Get(kUnreachableBaseUri);

   EXPECT_EQ(result1.status, OndasConfigLoader::Status::Error);
   EXPECT_EQ(result2.status, OndasConfigLoader::Status::Error);
   EXPECT_EQ(result1.config, nullptr);
   EXPECT_EQ(result2.config, nullptr);
}

} // namespace scwx::config
