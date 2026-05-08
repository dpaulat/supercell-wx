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

} // namespace scwx::qt::main::internal
