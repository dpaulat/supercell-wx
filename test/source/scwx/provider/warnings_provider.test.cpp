#include <scwx/provider/warnings_provider.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace provider
{

static const std::string& kDefaultUrl {"https://warnings.allisonhouse.com"};
static const std::string& kAlternateUrl {"http://warnings.cod.edu"};

class WarningsProviderTest : public testing::TestWithParam<std::string>
{
};
TEST_P(WarningsProviderTest, ListFiles)
{
   WarningsProvider provider(GetParam());

   auto [newObjects, totalObjects] = provider.ListFiles();

   EXPECT_GT(newObjects, 0);
   EXPECT_GT(totalObjects, 0);
   EXPECT_EQ(newObjects, totalObjects);
}

TEST_P(WarningsProviderTest, LoadUpdatedFiles)
{
   WarningsProvider provider(GetParam());

   auto [newObjects, totalObjects] = provider.ListFiles();
   auto updatedFiles               = provider.LoadUpdatedFiles();

   EXPECT_GT(newObjects, 0);
   EXPECT_GT(totalObjects, 0);
   EXPECT_EQ(newObjects, totalObjects);
   EXPECT_EQ(updatedFiles.size(), newObjects);

   auto [newObjects2, totalObjects2] = provider.ListFiles();
   auto updatedFiles2                = provider.LoadUpdatedFiles();

   // There should be no more than 2 updated warnings files since the last query
   // (assumption that the previous newest file was updated, and a new file was
   // created on the hour)
   EXPECT_LE(newObjects2, 2);
   EXPECT_EQ(updatedFiles2.size(), newObjects2);

   // The total number of objects may have changed, since the oldest file could
   // have dropped off the list
   EXPECT_GT(totalObjects2, 0);
}

INSTANTIATE_TEST_SUITE_P(WarningsProvider,
                         WarningsProviderTest,
                         testing::Values(kDefaultUrl, kAlternateUrl));

} // namespace provider
} // namespace scwx
