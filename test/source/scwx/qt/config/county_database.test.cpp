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

TEST_P(CountyDatabaseTest, CountyName)
{
   auto& [id, name] = GetParam();

   std::string actualName = CountyDatabase::GetCountyName(id);

   EXPECT_EQ(actualName, name);
}

INSTANTIATE_TEST_SUITE_P(
   CountyDatabase,
   CountyDatabaseTest,
   testing::Values(std::make_pair("AZC013", "Maricopa"),
                   std::make_pair("MOC183", "St. Charles"),
                   std::make_pair("TXZ211", "Austin"),
                   std::make_pair("GMZ335", "Galveston Bay"),
                   std::make_pair("ANZ338", "New York Harbor")));

} // namespace config
} // namespace qt
} // namespace scwx
