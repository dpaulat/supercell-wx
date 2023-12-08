#include <scwx/qt/config/county_database.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace qt
{
namespace config
{

class CountyDatabaseTest :
    public testing::TestWithParam<std::pair<std::string, std::string>>
{
   virtual void SetUp() { scwx::qt::config::CountyDatabase::Initialize(); }
};

class CountyCountTest :
    public testing::TestWithParam<std::pair<std::string, std::size_t>>
{
   virtual void SetUp() { scwx::qt::config::CountyDatabase::Initialize(); }
};

TEST_P(CountyDatabaseTest, CountyName)
{
   auto& [id, name] = GetParam();

   std::string actualName = CountyDatabase::GetCountyName(id);

   EXPECT_EQ(actualName, name);
}

TEST_P(CountyCountTest, State)
{
   auto& [state, size] = GetParam();

   auto counties = CountyDatabase::GetCounties(state);

   EXPECT_EQ(counties.size(), size);
}

INSTANTIATE_TEST_SUITE_P(
   CountyDatabase,
   CountyDatabaseTest,
   testing::Values(std::make_pair("AZC013", "Maricopa"),
                   std::make_pair("MOC183", "St. Charles"),
                   std::make_pair("TXZ211", "Austin"),
                   std::make_pair("GMZ335", "Galveston Bay"),
                   std::make_pair("ANZ338", "New York Harbor")));

INSTANTIATE_TEST_SUITE_P(CountyDatabase,
                         CountyCountTest,
                         testing::Values(std::make_pair("AZ", 15),
                                         std::make_pair("MO", 115),
                                         std::make_pair("TX", 254),
                                         std::make_pair("GM", 0),
                                         std::make_pair("AN", 0)));

} // namespace config
} // namespace qt
} // namespace scwx
