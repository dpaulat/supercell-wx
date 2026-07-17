#include <scwx/common/sites.hpp>

#include <algorithm>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>

namespace scwx::common
{

namespace
{

struct RadarIdTransitionRule
{
   std::string                           originalRadarId;
   std::string                           canonicalRadarId;
   std::chrono::system_clock::time_point transitionStart;
   std::chrono::system_clock::time_point transitionEnd;
};

struct OriginalRadarIdTag
{
};
struct CanonicalRadarIdTag
{
};

} // namespace

using namespace std::chrono;

namespace bmi = boost::multi_index;

static const bmi::multi_index_container<
   RadarIdTransitionRule,
   bmi::indexed_by<
      bmi::hashed_unique<bmi::tag<OriginalRadarIdTag>,
                         bmi::member<RadarIdTransitionRule,
                                     std::string,
                                     &RadarIdTransitionRule::originalRadarId>>,
      bmi::hashed_unique<
         bmi::tag<CanonicalRadarIdTag>,
         bmi::member<RadarIdTransitionRule,
                     std::string,
                     &RadarIdTransitionRule::canonicalRadarId>>>>
   kRadarIdTransitionRules_ = {{
      {
         .originalRadarId  = "TPBI",
         .canonicalRadarId = "TDJT",
         .transitionStart  = std::chrono::sys_days {2026y / August / 3d},
         .transitionEnd    = std::chrono::sys_days {2026y / August / 17d},
      },
   }};

std::string GetCanonicalRadarId(const std::string& radarId)
{
   // Find by original radar ID
   const auto& originalIndex =
      kRadarIdTransitionRules_.get<OriginalRadarIdTag>();

   const auto it = originalIndex.find(radarId);
   if (it != originalIndex.end())
   {
      return it->canonicalRadarId;
   }

   return radarId;
}

std::string GetSiteId(const std::string& radarId)
{
   std::string siteId = radarId;

   // Shorten only if radarId does not contain digits
   if (!std::ranges::any_of(
          radarId,
          [](char c) { return std::isdigit(static_cast<unsigned char>(c)); }))
   {
      const std::size_t siteIdIndex =
         std::max<std::size_t>(radarId.length(), 3u) - 3u;
      siteId = radarId.substr(siteIdIndex);
   }

   return siteId;
}

std::unordered_map<std::string, std::string> GetSiteIdMap()
{
   std::unordered_map<std::string, std::string> siteIdMap;

   for (const auto& rule : kRadarIdTransitionRules_)
   {
      siteIdMap.emplace(GetSiteId(rule.originalRadarId), rule.canonicalRadarId);
   }

   return siteIdMap;
}

std::vector<std::string>
GetRadarIdCandidates(const std::string&                          radarId,
                     const std::chrono::system_clock::time_point date)
{
   // Find by original or canonical radar ID
   static const auto& originalIndex =
      kRadarIdTransitionRules_.get<OriginalRadarIdTag>();
   static const auto& canonicalIndex =
      kRadarIdTransitionRules_.get<CanonicalRadarIdTag>();

   const RadarIdTransitionRule* transitionRule = nullptr;
   std::vector<std::string>     candidates {};

   const auto originalIt = originalIndex.find(radarId);
   if (originalIt != originalIndex.end())
   {
      transitionRule = &*originalIt;
   }
   else
   {
      const auto canonicalIt = canonicalIndex.find(radarId);
      if (canonicalIt != canonicalIndex.end())
      {
         transitionRule = &*canonicalIt;
      }
   }

   if (transitionRule != nullptr)
   {
      // If the date is after the transition start date, or unspecified, add the
      // canonical ID
      if (date >= transitionRule->transitionStart ||
          date == std::chrono::system_clock::time_point {})
      {
         candidates.push_back(transitionRule->canonicalRadarId);
      }

      // If the date is before the transition end date, or unspecified, add the
      // original ID
      if (date < transitionRule->transitionEnd ||
          date == std::chrono::system_clock::time_point {})
      {
         candidates.push_back(transitionRule->originalRadarId);
      }
   }
   else
   {
      candidates.push_back(radarId);
   }

   return candidates;
}

} // namespace scwx::common
