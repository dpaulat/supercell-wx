#include <scwx/qt/map/map_pane_splitter_state.hpp>

#include <gtest/gtest.h>

#include <QList>

using scwx::qt::map::MapPaneSplitterStateSizesAllPositive;

TEST(MapPaneSplitterState, empty_not_positive)
{
   EXPECT_FALSE(MapPaneSplitterStateSizesAllPositive({}));
}

TEST(MapPaneSplitterState, zero_or_negative_rejected)
{
   EXPECT_FALSE(MapPaneSplitterStateSizesAllPositive({0, 100}));
   EXPECT_FALSE(MapPaneSplitterStateSizesAllPositive({-1, 100}));
   EXPECT_FALSE(MapPaneSplitterStateSizesAllPositive({100, 0}));
}

TEST(MapPaneSplitterState, all_positive_ok)
{
   EXPECT_TRUE(MapPaneSplitterStateSizesAllPositive({1}));
   EXPECT_TRUE(MapPaneSplitterStateSizesAllPositive({100, 200, 1}));
}
