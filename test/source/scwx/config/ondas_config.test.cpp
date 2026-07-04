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

   const std::string listFile = ondasConfig.list_file();
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

TEST(OndasConfigTest, GetTimePointFromFilename)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   constexpr auto expectedTime = sys_days {2026y / January / 31d} + 18h + 30min;

   EXPECT_EQ(OndasConfig::GetTimePointFromFilename("20260131_1830"),
             expectedTime);
   EXPECT_EQ(OndasConfig::GetTimePointFromFilename("KILN_20260131_1830"),
             expectedTime);
   EXPECT_EQ(OndasConfig::GetTimePointFromFilename("KILN_20260131_1830.raw"),
             expectedTime);
}

TEST(OndasConfigTest, GetTimePointFromFilenameInvalid)
{
   constexpr std::chrono::system_clock::time_point expectedTime {};

   EXPECT_EQ(OndasConfig::GetTimePointFromFilename("invalid"), expectedTime);
   EXPECT_EQ(OndasConfig::GetTimePointFromFilename("2026-01-31_1830"),
             expectedTime);
}

} // namespace scwx::config
