#include <scwx/qt/settings/settings_container.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace scwx
{
namespace qt
{
namespace settings
{

TEST(SettingsContainerTest, Integer)
{
   SettingsContainer<std::vector<int64_t>> intContainer {
      "std::vector<int64_t>"};
   intContainer.SetDefault({42, 5, 63});
   intContainer.SetElementMinimum(4);
   intContainer.SetElementMaximum(70);
   intContainer.SetValueToDefault();

   EXPECT_EQ(intContainer.name(), "std::vector<int64_t>");
   EXPECT_THAT(intContainer.GetValue(), ::testing::ElementsAre(42, 5, 63));
   EXPECT_EQ(intContainer.SetValueOrDefault({50, 0, 80}), false);
   EXPECT_THAT(intContainer.GetValue(), ::testing::ElementsAre(50, 4, 70));
   EXPECT_EQ(intContainer.SetValueOrDefault({10, 20, 30}), true);
   EXPECT_THAT(intContainer.GetValue(), ::testing::ElementsAre(10, 20, 30));
}

} // namespace settings
} // namespace qt
} // namespace scwx
