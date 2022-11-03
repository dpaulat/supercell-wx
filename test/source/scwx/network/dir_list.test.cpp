#include <scwx/network/dir_list.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace network
{

static const std::string& kDefaultUrl {"https://warnings.allisonhouse.com"};
static const std::string& kAlternateUrl {"http://warnings.cod.edu"};

TEST(DirList, GetDefaultUrl)
{
   auto records = DirList(kDefaultUrl);

   EXPECT_GT(records.size(), 0);
}

TEST(DirList, GetAlternateUrl)
{
   auto records = DirList(kAlternateUrl);

   EXPECT_GT(records.size(), 0);
}

} // namespace network
} // namespace scwx
