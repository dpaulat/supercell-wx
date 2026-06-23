#include <scwx/qt/map/map_basemap_share.hpp>

#include <cmath>
#include <limits>

namespace scwx::qt::map
{

namespace
{

constexpr double kViewEpsilon = 1e-6;

bool NearlyEqual(const double a, const double b) noexcept
{
   return std::abs(a - b) <= kViewEpsilon;
}

} // namespace

bool MapViewsCompatibleForBasemapShare(const MapViewSnapshot& leader,
                                       const MapViewSnapshot& follower) noexcept
{
   if (leader.styleName_ != follower.styleName_)
   {
      return false;
   }

   if (leader.renderWidth_ != follower.renderWidth_ ||
       leader.renderHeight_ != follower.renderHeight_)
   {
      return false;
   }

   return NearlyEqual(leader.latitude_, follower.latitude_) &&
          NearlyEqual(leader.longitude_, follower.longitude_) &&
          NearlyEqual(leader.zoom_, follower.zoom_) &&
          NearlyEqual(leader.bearing_, follower.bearing_) &&
          NearlyEqual(leader.pitch_, follower.pitch_);
}

BasemapShareDecision ResolveBasemapShareDecision(
   const std::size_t                   paneIndex,
   const std::size_t                   mapCount,
   const bool                          viewLinked,
   const bool                          poppedOut,
   const MapViewSnapshot&              paneView,
   const std::size_t                   activePaneIndex,
   const std::vector<MapViewSnapshot>& allViews,
   const std::vector<bool>&            viewLinkedFlags,
   const std::vector<bool>&            poppedOutFlags) noexcept
{
   BasemapShareDecision decision {.role_         = BasemapPaneRole::Independent,
                                  .leaderIndex_ = paneIndex};

   if (mapCount < 2 || !viewLinked || poppedOut ||
       paneIndex >= allViews.size() || paneIndex >= viewLinkedFlags.size() ||
       paneIndex >= poppedOutFlags.size())
   {
      return decision;
   }

   std::size_t leaderIndex = activePaneIndex;
   if (leaderIndex >= mapCount || !viewLinkedFlags[leaderIndex] ||
       poppedOutFlags[leaderIndex])
   {
      leaderIndex = std::numeric_limits<std::size_t>::max();
      for (std::size_t i = 0; i < mapCount; ++i)
      {
         if (i < viewLinkedFlags.size() && viewLinkedFlags[i] &&
             i < poppedOutFlags.size() && !poppedOutFlags[i])
         {
            leaderIndex = i;
            break;
         }
      }
   }

   if (leaderIndex >= mapCount || leaderIndex >= allViews.size())
   {
      return decision;
   }

   if (!MapViewsCompatibleForBasemapShare(allViews[leaderIndex], paneView))
   {
      return decision;
   }

   decision.leaderIndex_ = leaderIndex;
   if (paneIndex == leaderIndex)
   {
      decision.role_ = BasemapPaneRole::Leader;
   }
   else
   {
      decision.role_ = BasemapPaneRole::Follower;
   }

   return decision;
}

void MapBasemapShareState::Reset(const std::size_t /* mapCount */)
{
   basemapGeneration_ = 0;
   lastLeaderIndex_.reset();
}

void MapBasemapShareState::NotifyBasemapRendered(const std::size_t leaderIndex)
{
   lastLeaderIndex_ = leaderIndex;
   ++basemapGeneration_;
}

} // namespace scwx::qt::map
