#include <scwx/qt/manager/product_datastore.hpp>
#include <scwx/qt/types/radar_product_record.hpp>
#include <scwx/wsr88d/nexrad_file_factory.hpp>

#include <chrono>

#include <gtest/gtest.h>

namespace scwx::qt::manager
{

namespace
{

std::shared_ptr<types::RadarProductRecord>
CreateLevel2Record(std::chrono::system_clock::time_point time)
{
   const std::string filename = std::string(SCWX_TEST_DATA_DIR) +
                                "/nexrad/level2/Level2_KLSX_20210527_1757.ar2v";

   auto nexradFile = wsr88d::NexradFileFactory::Create(filename);
   auto record     = types::RadarProductRecord::Create(nexradFile);
   record->set_time(time);
   return record;
}

std::shared_ptr<types::RadarProductRecord>
CreateLevel3Record(std::chrono::system_clock::time_point time)
{
   const std::string filename =
      std::string(SCWX_TEST_DATA_DIR) +
      "/nexrad/level3/KLSX_SDUS23_N2QLSX_202112110250";

   auto nexradFile = wsr88d::NexradFileFactory::Create(filename);
   auto record     = types::RadarProductRecord::Create(nexradFile);
   record->set_time(time);
   return record;
}

} // namespace

TEST(ProductDatastore, SetCacheLimitMinimum)
{
   ProductDatastore datastore {};

   datastore.SetCacheLimit(1u);
   EXPECT_EQ(datastore.cache_limit(), 6u);

   datastore.SetCacheLimit(12u);
   EXPECT_EQ(datastore.cache_limit(), 12u);
}

TEST(ProductDatastore, StoreLevel2Dedup)
{
   ProductDatastore datastore {};

   using namespace std::chrono_literals;

   const auto time = std::chrono::floor<std::chrono::seconds>(
      std::chrono::system_clock::now());

   auto record = CreateLevel2Record(time);
   ASSERT_NE(record, nullptr);

   const auto storedRecord = datastore.Store(record);
   const auto cachedRecord = datastore.Store(record);

   EXPECT_EQ(storedRecord, cachedRecord);
   EXPECT_EQ(storedRecord, record);
}

TEST(ProductDatastore, StoreLevel3Dedup)
{
   ProductDatastore datastore {};

   using namespace std::chrono_literals;

   const auto time = std::chrono::floor<std::chrono::seconds>(
      std::chrono::system_clock::now());

   auto record = CreateLevel3Record(time);
   ASSERT_NE(record, nullptr);

   const auto storedRecord = datastore.Store(record);
   const auto cachedRecord = datastore.Store(record);

   EXPECT_EQ(storedRecord, cachedRecord);
   EXPECT_EQ(storedRecord, record);
}

TEST(ProductDatastore, FindLevel2RecordEntries)
{
   ProductDatastore datastore {};

   using namespace std::chrono_literals;

   const auto earlierTime = std::chrono::floor<std::chrono::seconds>(
      std::chrono::system_clock::now() - 10min);
   const auto laterTime = earlierTime + 5min;

   datastore.Store(CreateLevel2Record(earlierTime));
   datastore.Store(CreateLevel2Record(laterTime));

   const auto latestEntries = datastore.FindLevel2RecordEntries(
      std::chrono::system_clock::time_point {});
   ASSERT_EQ(latestEntries.size(), 1u);
   EXPECT_EQ(latestEntries.front().time, laterTime);

   const auto boundedEntries =
      datastore.FindLevel2RecordEntries(laterTime + 1min);
   ASSERT_EQ(boundedEntries.size(), 2u);
   EXPECT_EQ(boundedEntries.back().time, earlierTime);
   EXPECT_EQ(boundedEntries.front().time, laterTime);
}

TEST(ProductDatastore, FindLevel3RecordEntry)
{
   ProductDatastore datastore {};

   using namespace std::chrono_literals;

   const auto time = std::chrono::floor<std::chrono::seconds>(
      std::chrono::system_clock::now());

   auto record = CreateLevel3Record(time);
   ASSERT_NE(record, nullptr);

   datastore.Store(record);

   const auto product = record->radar_product();

   const auto latestEntry = datastore.FindLevel3RecordEntry(
      product, std::chrono::system_clock::time_point {});
   ASSERT_TRUE(latestEntry.has_value());
   EXPECT_EQ(latestEntry->time, time);

   const auto exactEntry = datastore.FindLevel3RecordEntry(product, time);
   ASSERT_TRUE(exactEntry.has_value());
   EXPECT_EQ(exactEntry->time, time);
}

TEST(ProductDatastore, GetCachedNexradFile)
{
   ProductDatastore datastore {};

   using namespace std::chrono_literals;

   const auto time = std::chrono::floor<std::chrono::seconds>(
      std::chrono::system_clock::now());

   auto record = CreateLevel2Record(time);
   ASSERT_NE(record, nullptr);

   datastore.Store(record);

   const auto cachedFile = datastore.GetCachedNexradFile(
      common::RadarProductGroup::Level2, {}, time);
   EXPECT_EQ(cachedFile, record->nexrad_file());
}

TEST(ProductDatastore, GetCachedNexradFileSubsecondLookup)
{
   ProductDatastore datastore {};

   using namespace std::chrono_literals;

   const auto time = std::chrono::floor<std::chrono::seconds>(
      std::chrono::system_clock::now());

   auto record = CreateLevel2Record(time);
   ASSERT_NE(record, nullptr);

   datastore.Store(record);

   const auto cachedFile = datastore.GetCachedNexradFile(
      common::RadarProductGroup::Level2, {}, time + 500ms);
   EXPECT_EQ(cachedFile, record->nexrad_file());
}

} // namespace scwx::qt::manager
