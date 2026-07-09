#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class QRhi;
class QRhiTexture;

namespace scwx::qt::map
{

struct MapViewSnapshot
{
   double      latitude_ {};
   double      longitude_ {};
   double      zoom_ {};
   double      bearing_ {};
   double      pitch_ {};
   std::string styleName_ {};
   int         renderWidth_ {};
   int         renderHeight_ {};
};

enum class BasemapPaneRole
{
   Independent,
   Leader,
   Follower
};

struct BasemapShareDecision
{
   BasemapPaneRole role_ {BasemapPaneRole::Independent};
   std::size_t     leaderIndex_ {0};
};

struct MapBasemapShareCallbacks
{
   std::function<BasemapShareDecision(std::size_t, const MapViewSnapshot&)>
                                            resolve_ {};
   std::function<QRhiTexture*(std::size_t)> leaderTexture_ {};
   std::function<QRhi*(std::size_t)>       leaderRhi_ {};
   std::function<std::uint64_t()>          basemapGeneration_ {};
   std::function<void(std::size_t)>         notifyRendered_ {};
   std::function<void(std::size_t)>         notifyOverlayDirty_ {};
};

[[nodiscard]] bool
MapViewsCompatibleForBasemapShare(const MapViewSnapshot& leader,
                                  const MapViewSnapshot& follower) noexcept;

[[nodiscard]] BasemapShareDecision
ResolveBasemapShareDecision(std::size_t                         paneIndex,
                            std::size_t                         mapCount,
                            bool                                viewLinked,
                            bool                                poppedOut,
                            const MapViewSnapshot&              paneView,
                            std::size_t                         activePaneIndex,
                            const std::vector<MapViewSnapshot>& allViews,
                            const std::vector<bool>&            viewLinkedFlags,
                            const std::vector<bool>& poppedOutFlags) noexcept;

class MapBasemapShareState
{
public:
   void Reset(std::size_t mapCount);

   void NotifyBasemapRendered(std::size_t leaderIndex);

   [[nodiscard]] std::uint64_t basemapGeneration() const noexcept
   {
      return basemapGeneration_;
   }

   [[nodiscard]] std::optional<std::size_t> lastLeaderIndex() const noexcept
   {
      return lastLeaderIndex_;
   }

private:
   std::uint64_t              basemapGeneration_ {0};
   std::optional<std::size_t> lastLeaderIndex_ {};
};

} // namespace scwx::qt::map
