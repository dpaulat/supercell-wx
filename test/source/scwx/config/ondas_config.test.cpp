#include <scwx/config/ondas_config.hpp>

#include <gtest/gtest.h>
#include <sstream>

namespace scwx::config
{

TEST(OndasConfigTest, ListFile)
{
   static const std::string kConfigContent = R"(
ListFile: custom_dir.list
)";

   OndasConfig ondasConfig {};

   std::istringstream is {kConfigContent};
   ondasConfig.Parse(is);

   std::string listFile = ondasConfig.list_file();
   EXPECT_EQ(listFile, "custom_dir.list");
}

TEST(OndasConfigTest, Sites)
{
   static const std::string kConfigContent = R"(
Site: KILN
Site: KTLX
Site: KDVN
)";

   OndasConfig ondasConfig {};

   std::istringstream is {kConfigContent};
   ondasConfig.Parse(is);

   std::vector<std::string> sites = ondasConfig.sites();
   ASSERT_EQ(sites.size(), 3);
   EXPECT_EQ(sites[0], "KILN");
   EXPECT_EQ(sites[1], "KTLX");
   EXPECT_EQ(sites[2], "KDVN");
}

TEST(OndasConfigTest, ApplySiteSubstitution)
{
   static const std::string kConfigContent = R"(
Product: N0B SSSS/N0B
Product: N0Q ssss/N0Q
Product: N0R SSS/N0R
Product: N0G sss/test
)";

   OndasConfig ondasConfig {};

   std::istringstream is {kConfigContent};
   ondasConfig.Parse(is);

   std::string result = ondasConfig.ApplySiteSubstitution("KILN", "N0B");
   EXPECT_EQ(result, "KILN/N0B");

   result = ondasConfig.ApplySiteSubstitution("KTLX", "N0Q");
   EXPECT_EQ(result, "ktlx/N0Q");

   result = ondasConfig.ApplySiteSubstitution("ABCD", "N0R");
   EXPECT_EQ(result, "BCD/N0R");

   result = ondasConfig.ApplySiteSubstitution("WXYZ", "N0G");
   EXPECT_EQ(result, "xyz/test");
}

} // namespace scwx::config
