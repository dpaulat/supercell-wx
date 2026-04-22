#include <scwx/qt/settings/ui_settings.hpp>

#include <gtest/gtest.h>

namespace scwx::qt::settings
{

TEST(UiSettingsTest, MapAnnotationStateDefaultsToEmpty)
{
   const UiSettings settings;

   EXPECT_EQ(settings.map_annotation_state().GetValue(), "");
}

TEST(UiSettingsTest, MapAnnotationStateParticipatesInEquality)
{
   UiSettings lhs;
   UiSettings rhs;

   EXPECT_TRUE(lhs == rhs);

   lhs.map_annotation_state().SetValue("{\"expanded\":true}");
   EXPECT_FALSE(lhs == rhs);

   rhs.map_annotation_state().SetValue("{\"expanded\":true}");
   EXPECT_TRUE(lhs == rhs);
}

TEST(UiSettingsTest, ShutdownCommitsStagedMapAnnotationState)
{
   UiSettings settings;

   EXPECT_TRUE(settings.map_annotation_state().StageValue("{\"tool_id\":5}"));
   EXPECT_EQ(settings.map_annotation_state().GetValue(), "");

   EXPECT_TRUE(settings.Shutdown());
   EXPECT_EQ(settings.map_annotation_state().GetValue(), "{\"tool_id\":5}");
}

} // namespace scwx::qt::settings
