#include <scwx/qt/main/theme_internal.hpp>

#include <gtest/gtest.h>

namespace scwx::qt::main::internal
{

TEST(ThemeTest, HasStyleArgument_NoArgs)
{
   EXPECT_FALSE(HasStyleArgument({"program"}));
}

TEST(ThemeTest, HasStyleArgument_StyleFlagOnly)
{
   EXPECT_TRUE(HasStyleArgument({"program", "-style"}));
}

TEST(ThemeTest, HasStyleArgument_StyleFlagWithValue)
{
   EXPECT_TRUE(HasStyleArgument({"program", "-style", "fusion"}));
}

TEST(ThemeTest, HasStyleArgument_MissingStyleFlag)
{
   EXPECT_FALSE(HasStyleArgument({"program", "--portable", "--help"}));
}

TEST(ThemeTest, FollowsSystemColorScheme_SystemStyles)
{
   EXPECT_TRUE(FollowsSystemColorScheme(types::UiStyle::Default));
   EXPECT_TRUE(FollowsSystemColorScheme(types::UiStyle::Fusion));
}

TEST(ThemeTest, FollowsSystemColorScheme_FixedStyles)
{
   EXPECT_FALSE(FollowsSystemColorScheme(types::UiStyle::FusionLight));
   EXPECT_FALSE(FollowsSystemColorScheme(types::UiStyle::FusionDark));
   EXPECT_FALSE(FollowsSystemColorScheme(types::UiStyle::FusionDarker));
   EXPECT_FALSE(FollowsSystemColorScheme(types::UiStyle::FusionCustom));
   EXPECT_FALSE(FollowsSystemColorScheme(types::UiStyle::Unknown));
}

} // namespace scwx::qt::main::internal
