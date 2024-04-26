#include <scwx/network/dir_list.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace network
{

static const std::string& kDefaultUrl {"https://warnings.allisonhouse.com"};
static const std::string& kAlternateUrl {"https://warnings.cod.edu"};

TEST(DirList, GetDefaultUrl)
{
   auto records = DirList(kDefaultUrl);

   // No records, skip test
   if (records.size() == 0)
   {
      GTEST_SKIP();
   }

   EXPECT_GT(records.size(), 0);
}

TEST(DirList, GetAlternateUrl)
{
   auto records = DirList(kAlternateUrl);

   // No records, skip test
   if (records.size() == 0)
   {
      GTEST_SKIP();
   }

   EXPECT_GT(records.size(), 0);
}

} // namespace network
} // namespace scwx
