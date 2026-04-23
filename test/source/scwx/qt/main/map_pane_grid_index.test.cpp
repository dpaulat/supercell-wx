// Row/col math must stay aligned with |BuildMapLayout| and
// |MapSlotByMapIndex| in main_window.cpp.
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

namespace
{
void SlotFor(std::size_t mapIndex, std::int64_t gridW, int& row, int& col)
{
   row = static_cast<int>(mapIndex / static_cast<std::size_t>(gridW));
   col = static_cast<int>(mapIndex % static_cast<std::size_t>(gridW));
}
} // namespace

TEST(MapPaneGridIndex, two_by_two_is_row_major)
{
   int                    r;
   int                    c;
   constexpr std::int64_t w = 2;
   SlotFor(0, w, r, c);
   EXPECT_EQ(r, 0);
   EXPECT_EQ(c, 0);
   SlotFor(1, w, r, c);
   EXPECT_EQ(r, 0);
   EXPECT_EQ(c, 1);
   SlotFor(2, w, r, c);
   EXPECT_EQ(r, 1);
   EXPECT_EQ(c, 0);
   SlotFor(3, w, r, c);
   EXPECT_EQ(r, 1);
   EXPECT_EQ(c, 1);
}

TEST(MapPaneGridIndex, one_column_stacks_rows)
{
   int                    r;
   int                    c;
   constexpr std::int64_t w = 1;
   SlotFor(0, w, r, c);
   EXPECT_EQ(r, 0);
   EXPECT_EQ(c, 0);
   SlotFor(1, w, r, c);
   EXPECT_EQ(r, 1);
   EXPECT_EQ(c, 0);
}
