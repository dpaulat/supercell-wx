#include <scwx/common/products.hpp>

#include <gtest/gtest.h>
#include <algorithm>

namespace scwx
{
namespace common
{

class GetLevel3ProductByAwipsIdTest :
    public testing::TestWithParam<std::pair<std::string, std::string>>
{
};

TEST(Products, GetLevel3AwipsIdsByProductTest)
{
   const auto& awipsIds = GetLevel3AwipsIdsByProduct("SDR");

   EXPECT_NE(std::find(awipsIds.cbegin(), awipsIds.cend(), "N0B"),
             awipsIds.cend());
   EXPECT_NE(std::find(awipsIds.cbegin(), awipsIds.cend(), "N1B"),
             awipsIds.cend());

   EXPECT_EQ(std::find(awipsIds.cbegin(), awipsIds.cend(), "N0Q"),
             awipsIds.cend());
}

TEST(Products, GetLevel3ProductsByCategoryTest)
{
   const auto& products = GetLevel3ProductsByCategory(Level3ProductCategory::Reflectivity);

   EXPECT_NE(std::find(products.cbegin(), products.cend(), "SDR"),
             products.cend());
   EXPECT_NE(std::find(products.cbegin(), products.cend(), "DR"),
             products.cend());

   EXPECT_EQ(std::find(products.cbegin(), products.cend(), "DV"),
             products.cend());
}

TEST_P(GetLevel3ProductByAwipsIdTest, AwipsIdTest)
{
   auto& [awipsId, productName] = GetParam();

   std::string product {GetLevel3ProductByAwipsId(awipsId)};
   EXPECT_EQ(product, productName);
}

INSTANTIATE_TEST_SUITE_P(Products,
                         GetLevel3ProductByAwipsIdTest,
                         testing::Values(std::make_pair("N0Q", "DR"),
                                         std::make_pair("N0B", "SDR"),
                                         std::make_pair("XXX", "?")));

} // namespace common
} // namespace scwx
