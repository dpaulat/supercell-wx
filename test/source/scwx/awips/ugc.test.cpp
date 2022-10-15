#include <scwx/awips/ugc.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace awips
{

static const std::string logPrefix_ = "scwx::awips::ugc.test";

// Test cases are used from examples at the end of NWSI 10-1702

TEST(Ugc, EntireState1)
{
   Ugc                      ugc;
   std::vector<std::string> ugcString {"NDZ001>054-222115-"};

   ugc.Parse(ugcString);

   auto states     = ugc.states();
   auto fipsIds    = ugc.fips_ids();
   auto expiration = ugc.product_expiration();

   EXPECT_EQ(states, std::vector<std::string> {"ND"});
   ASSERT_EQ(fipsIds.size(), 54u);
   EXPECT_EQ(fipsIds[0], "NDZ001");
   EXPECT_EQ(fipsIds[1], "NDZ002");
   EXPECT_EQ(fipsIds[52], "NDZ053");
   EXPECT_EQ(fipsIds[53], "NDZ054");
   EXPECT_EQ(expiration, "222115");
}

TEST(Ugc, EntireState2)
{
   Ugc                      ugc;
   std::vector<std::string> ugcString {"COZALL-220000-"};

   ugc.Parse(ugcString);

   auto states     = ugc.states();
   auto fipsIds    = ugc.fips_ids();
   auto expiration = ugc.product_expiration();

   EXPECT_EQ(states, std::vector<std::string> {"CO"});
   ASSERT_EQ(fipsIds.size(), 1u);
   EXPECT_EQ(fipsIds[0], "COZ000");
   EXPECT_EQ(expiration, "220000");
}

TEST(Ugc, EntireState3)
{
   Ugc                      ugc;
   std::vector<std::string> ugcString {"AKZALL-191846-"};

   ugc.Parse(ugcString);

   auto states     = ugc.states();
   auto fipsIds    = ugc.fips_ids();
   auto expiration = ugc.product_expiration();

   EXPECT_EQ(states, std::vector<std::string> {"AK"});
   ASSERT_EQ(fipsIds.size(), 1u);
   EXPECT_EQ(fipsIds[0], "AKZ000");
   EXPECT_EQ(expiration, "191846");
}

TEST(Ugc, EntireState5)
{
   Ugc                      ugc;
   std::vector<std::string> ugcString {"IAC000-060015-"};

   ugc.Parse(ugcString);

   auto states     = ugc.states();
   auto fipsIds    = ugc.fips_ids();
   auto expiration = ugc.product_expiration();

   EXPECT_EQ(states, std::vector<std::string> {"IA"});
   ASSERT_EQ(fipsIds.size(), 1u);
   EXPECT_EQ(fipsIds[0], "IAC000");
   EXPECT_EQ(expiration, "060015");
}

TEST(Ugc, PartOfState1)
{
   Ugc                      ugc;
   std::vector<std::string> ugcString {
      "DCZ001-MDZ003>007-009>011-013-014-016>018-501-502-VAZ021-025>031-",
      "036>040-042-050>057-501-502-WVZ050>055-501>504-182200-"};

   ugc.Parse(ugcString);

   auto states     = ugc.states();
   auto fipsIds    = ugc.fips_ids();
   auto expiration = ugc.product_expiration();

   const std::vector<std::string> expectedStates {"DC", "MD", "VA", "WV"};

   EXPECT_EQ(states, expectedStates);
   ASSERT_EQ(fipsIds.size(), 50u);
   EXPECT_EQ(fipsIds[0], "DCZ001");
   EXPECT_EQ(fipsIds[1], "MDZ003");
   EXPECT_EQ(fipsIds[2], "MDZ004");
   EXPECT_EQ(fipsIds[5], "MDZ007");
   EXPECT_EQ(fipsIds[15], "MDZ502");
   EXPECT_EQ(fipsIds[16], "VAZ021");
   EXPECT_EQ(fipsIds[17], "VAZ025");
   EXPECT_EQ(fipsIds[23], "VAZ031");
   EXPECT_EQ(fipsIds[39], "VAZ502");
   EXPECT_EQ(fipsIds[40], "WVZ050");
   EXPECT_EQ(fipsIds[45], "WVZ055");
   EXPECT_EQ(fipsIds[49], "WVZ504");
   EXPECT_EQ(expiration, "182200");
}

TEST(Ugc, PartOfState3)
{
   Ugc                      ugc;
   std::vector<std::string> ugcString {"LHZ349-363-202300-"};

   ugc.Parse(ugcString);

   auto states     = ugc.states();
   auto fipsIds    = ugc.fips_ids();
   auto expiration = ugc.product_expiration();

   EXPECT_EQ(states, std::vector<std::string> {"LH"});
   ASSERT_EQ(fipsIds.size(), 2u);
   EXPECT_EQ(fipsIds[0], "LHZ349");
   EXPECT_EQ(fipsIds[1], "LHZ363");
   EXPECT_EQ(expiration, "202300");
}

} // namespace awips
} // namespace scwx
