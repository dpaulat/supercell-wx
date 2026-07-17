#include <scwx/common/sites.hpp>

#include <gtest/gtest.h>

namespace scwx::common
{

using namespace std::chrono;
using namespace std::chrono_literals;

namespace
{

class GetCanonicalRadarIdTest :
    public testing::TestWithParam<std::pair<std::string, std::string>>
{
};

class GetRadarIdCandidatesTest :
    public testing::TestWithParam<
       std::tuple<std::string,
                  std::string,
                  std::chrono::system_clock::time_point,
                  std::chrono::system_clock::time_point>>
{
};

} // namespace

TEST_P(GetCanonicalRadarIdTest, CanonicalRadarIdTest)
{
   const auto& [originalRadarId, canonicalRadarId] = GetParam();

   const std::string originalRadarIdResult =
      GetCanonicalRadarId(originalRadarId);
   const std::string canonicalRadarIdResult =
      GetCanonicalRadarId(canonicalRadarId);

   EXPECT_EQ(originalRadarIdResult, canonicalRadarId);
   EXPECT_EQ(canonicalRadarIdResult, canonicalRadarId);
}

INSTANTIATE_TEST_SUITE_P(Sites,
                         GetCanonicalRadarIdTest,
                         testing::Values(std::make_pair("KLSX", "KLSX"),
                                         std::make_pair("TPBI", "TDJT")));

TEST(Sites, GetRadarIdCandidatesSingleCandidateTest)
{
   const std::vector<std::string> candidates = GetRadarIdCandidates("KLSX");
   EXPECT_EQ(candidates.size(), 1);
   if (candidates.size() >= 1)
   {
      EXPECT_EQ(candidates[0], "KLSX");
   }
}

TEST_P(GetRadarIdCandidatesTest, RadarIdCandidatesTest)
{
   const auto& [originalRadarId,
                canonicalRadarId,
                transitionStart,
                transitionEnd] = GetParam();

   const std::vector<std::string> beforeCandidates = GetRadarIdCandidates(
      originalRadarId, transitionStart - std::chrono::days {1});
   const std::vector<std::string> startCandidates =
      GetRadarIdCandidates(originalRadarId, transitionStart);
   const std::vector<std::string> endCandidates =
      GetRadarIdCandidates(originalRadarId, transitionEnd);
   const std::vector<std::string> afterCandidates = GetRadarIdCandidates(
      originalRadarId, transitionEnd + std::chrono::days {1});
   const std::vector<std::string> allCandidates1 =
      GetRadarIdCandidates(originalRadarId);
   const std::vector<std::string> allCandidates2 =
      GetRadarIdCandidates(canonicalRadarId);

   EXPECT_EQ(beforeCandidates.size(), 1);
   EXPECT_EQ(startCandidates.size(), 2); // Inclusive start date
   EXPECT_EQ(endCandidates.size(), 1);   // Exclusive end date
   EXPECT_EQ(afterCandidates.size(), 1);
   EXPECT_EQ(allCandidates1.size(), 2);
   EXPECT_EQ(allCandidates2.size(), 2);

   if (beforeCandidates.size() >= 1)
   {
      EXPECT_EQ(beforeCandidates[0], originalRadarId);
   }
   if (startCandidates.size() >= 2)
   {
      EXPECT_EQ(startCandidates[0], canonicalRadarId);
      EXPECT_EQ(startCandidates[1], originalRadarId);
   }
   if (endCandidates.size() >= 1)
   {
      EXPECT_EQ(endCandidates[0], canonicalRadarId);
   }
   if (afterCandidates.size() >= 1)
   {
      EXPECT_EQ(afterCandidates[0], canonicalRadarId);
   }
   if (allCandidates1.size() >= 2)
   {
      EXPECT_EQ(allCandidates1[0], canonicalRadarId);
      EXPECT_EQ(allCandidates1[1], originalRadarId);
   }
   if (allCandidates2.size() >= 2)
   {
      EXPECT_EQ(allCandidates2[0], canonicalRadarId);
      EXPECT_EQ(allCandidates2[1], originalRadarId);
   }
}

INSTANTIATE_TEST_SUITE_P(Sites,
                         GetRadarIdCandidatesTest,
                         testing::Values(std::make_tuple(
                            "TPBI",
                            "TDJT",
                            std::chrono::system_clock::time_point(
                               std::chrono::sys_days {2026y / August / 3d}),
                            std::chrono::system_clock::time_point(
                               std::chrono::sys_days {2026y / August / 17d}))));

TEST(Sites, GetSiteIdMapTest)
{
   const std::unordered_map<std::string, std::string> siteIdMap =
      GetSiteIdMap();
   EXPECT_GE(siteIdMap.size(), 1);

   const auto it = siteIdMap.find("PBI");
   EXPECT_NE(it, siteIdMap.end());

   if (it != siteIdMap.end())
   {
      EXPECT_EQ(it->second, "TDJT");
   }
}

} // namespace scwx::common