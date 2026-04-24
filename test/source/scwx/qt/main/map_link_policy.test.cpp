#include <scwx/qt/main/map_link_policy.hpp>

#include <gtest/gtest.h>

#include <cstddef>

namespace
{
// Non-null address only for pointer identity; never dereferenced.
const char dummy {0};
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
const scwx::qt::map::MapWidget* const kNonNullSource {
   reinterpret_cast<const scwx::qt::map::MapWidget*>(&dummy)};
} // namespace

TEST(MapLinkPolicy, single_pane_allows_missing_signal_source)
{
   using scwx::qt::main::ShouldApplyLinkedMapParameterSync;
   EXPECT_TRUE(ShouldApplyLinkedMapParameterSync(0u, nullptr));
   EXPECT_TRUE(ShouldApplyLinkedMapParameterSync(1u, nullptr));
}

TEST(MapLinkPolicy, multi_pane_requires_signal_source)
{
   using scwx::qt::main::ShouldApplyLinkedMapParameterSync;
   EXPECT_FALSE(ShouldApplyLinkedMapParameterSync(2u, nullptr));
   EXPECT_TRUE(ShouldApplyLinkedMapParameterSync(2u, kNonNullSource));
}
