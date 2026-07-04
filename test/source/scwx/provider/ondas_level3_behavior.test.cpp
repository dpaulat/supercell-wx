#include <scwx/config/ondas_config.hpp>
#include <scwx/provider/ondas_level3_behavior.hpp>

#include <gtest/gtest.h>
#include <sstream>

namespace scwx::provider
{

static std::shared_ptr<config::OndasConfig> MakeTestOndasConfig()
{
   static const std::string kConfigContent = R"(
ListFile: dir.list
Product: N0R sss/N0R
)";

   auto               config = std::make_shared<config::OndasConfig>();
   std::istringstream is {kConfigContent};
   config->Parse(is);
   return config;
}

TEST(OndasLevel3BehaviorTest, GetTimePointFromKey)
{
   using namespace std::chrono;
   using sys_days = time_point<system_clock, days>;

   constexpr auto expectedTime = sys_days {2026y / January / 31d} + 18h + 30min;

   EXPECT_EQ(OndasLevel3Behavior::GetTimePointFromKey("20260131_1830"),
             expectedTime);
}

TEST(OndasLevel3BehaviorTest, GetFileUrl)
{
   const auto config = MakeTestOndasConfig();

   const OndasLevel3Behavior behavior(
      "https://example.com", "KILN", "N0R", config);

   EXPECT_EQ(behavior.GetFileUrl("20260131_1830"),
             "https://example.com/iln/N0R/20260131_1830");
}

TEST(OndasLevel3BehaviorTest, DateArchiveNotAvailable)
{
   const auto config = MakeTestOndasConfig();

   const OndasLevel3Behavior behavior(
      "https://example.com", "KILN", "N0R", config);

   EXPECT_FALSE(behavior.date_archive_available());
}

} // namespace scwx::provider
