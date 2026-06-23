#include <scwx/qt/map/map_basemap_share.hpp>

#include <gtest/gtest.h>

#include <vector>

namespace scwx::qt::map
{

namespace
{

MapViewSnapshot MakeView(const std::string& style,
                         const int          width  = 800,
                         const int          height = 600)
{
   return MapViewSnapshot {.latitude_     = 38.0,
                           .longitude_    = -90.0,
                           .zoom_         = 8.0,
                           .bearing_      = 0.0,
                           .pitch_        = 0.0,
                           .styleName_    = style,
                           .renderWidth_  = width,
                           .renderHeight_ = height};
}

} // namespace

TEST(MapBasemapShare, compatible_matching_views)
{
   const MapViewSnapshot leader   = MakeView("Streets");
   const MapViewSnapshot follower = MakeView("Streets");
   EXPECT_TRUE(MapViewsCompatibleForBasemapShare(leader, follower));
}

TEST(MapBasemapShare, incompatible_style_or_size)
{
   const MapViewSnapshot leader        = MakeView("Streets", 800, 600);
   const MapViewSnapshot followerStyle = MakeView("Satellite", 800, 600);
   const MapViewSnapshot followerSize  = MakeView("Streets", 640, 480);

   EXPECT_FALSE(MapViewsCompatibleForBasemapShare(leader, followerStyle));
   EXPECT_FALSE(MapViewsCompatibleForBasemapShare(leader, followerSize));
}

TEST(MapBasemapShare, leader_is_active_linked_pane)
{
   const std::vector<MapViewSnapshot> views {MakeView("Streets"),
                                             MakeView("Streets")};
   const std::vector<bool>            linked {true, true};
   const std::vector<bool>            popped {false, false};

   const BasemapShareDecision leaderDecision = ResolveBasemapShareDecision(
      0u, 2u, true, false, views[0], 0u, views, linked, popped);
   EXPECT_EQ(leaderDecision.role_, BasemapPaneRole::Leader);
   EXPECT_EQ(leaderDecision.leaderIndex_, 0u);

   const BasemapShareDecision followerDecision = ResolveBasemapShareDecision(
      1u, 2u, true, false, views[1], 0u, views, linked, popped);
   EXPECT_EQ(followerDecision.role_, BasemapPaneRole::Follower);
   EXPECT_EQ(followerDecision.leaderIndex_, 0u);
}

TEST(MapBasemapShare, popped_or_unlinked_pane_is_independent)
{
   const std::vector<MapViewSnapshot> views {MakeView("Streets"),
                                             MakeView("Streets")};
   const std::vector<bool>            linked {true, true};
   const std::vector<bool>            popped {false, true};

   const BasemapShareDecision decision = ResolveBasemapShareDecision(
      1u, 2u, true, true, views[1], 0u, views, linked, popped);
   EXPECT_EQ(decision.role_, BasemapPaneRole::Independent);
}

TEST(MapBasemapShare, generation_increments_on_leader_render)
{
   MapBasemapShareState state {};
   state.Reset(4);
   EXPECT_EQ(state.basemapGeneration(), 0u);
   EXPECT_FALSE(state.lastLeaderIndex().has_value());

   state.NotifyBasemapRendered(2u);
   EXPECT_EQ(state.basemapGeneration(), 1u);
   ASSERT_TRUE(state.lastLeaderIndex().has_value());
   EXPECT_EQ(*state.lastLeaderIndex(), 2u);
}

TEST(MapBasemapShare, nine_pane_grid_active_leader)
{
   std::vector<MapViewSnapshot> views;
   views.reserve(9);
   for (int i = 0; i < 9; ++i)
   {
      views.push_back(MakeView("Streets"));
   }

   const std::vector<bool> linked(9, true);
   const std::vector<bool> popped(9, false);

   const BasemapShareDecision pane4 = ResolveBasemapShareDecision(
      4u, 9u, true, false, views[4], 4u, views, linked, popped);
   EXPECT_EQ(pane4.role_, BasemapPaneRole::Leader);
   EXPECT_EQ(pane4.leaderIndex_, 4u);

   const BasemapShareDecision pane8 = ResolveBasemapShareDecision(
      8u, 9u, true, false, views[8], 4u, views, linked, popped);
   EXPECT_EQ(pane8.role_, BasemapPaneRole::Follower);
   EXPECT_EQ(pane8.leaderIndex_, 4u);
}

TEST(MapBasemapShare, active_unlinked_falls_back_to_first_linked)
{
   const std::vector<MapViewSnapshot> views {
      MakeView("Streets"), MakeView("Streets"), MakeView("Streets")};
   const std::vector<bool> linked {false, true, true};
   const std::vector<bool> popped(3, false);

   const BasemapShareDecision decision = ResolveBasemapShareDecision(
      2u, 3u, true, false, views[2], 0u, views, linked, popped);
   EXPECT_EQ(decision.role_, BasemapPaneRole::Follower);
   EXPECT_EQ(decision.leaderIndex_, 1u);
}

TEST(MapBasemapShare, view_epsilon_allows_tiny_drift)
{
   MapViewSnapshot leader   = MakeView("Streets");
   MapViewSnapshot follower = MakeView("Streets");
   follower.latitude_ += 1e-7;
   follower.longitude_ -= 1e-7;
   EXPECT_TRUE(MapViewsCompatibleForBasemapShare(leader, follower));
}

TEST(MapBasemapShare, single_pane_is_always_independent)
{
   const std::vector<MapViewSnapshot> views {MakeView("Streets")};
   const std::vector<bool>            linked {true};
   const std::vector<bool>            popped {false};

   const BasemapShareDecision decision = ResolveBasemapShareDecision(
      0u, 1u, true, false, views[0], 0u, views, linked, popped);
   EXPECT_EQ(decision.role_, BasemapPaneRole::Independent);
}

} // namespace scwx::qt::map
