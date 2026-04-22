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
   const UiSettings lhs;
   const UiSettings rhs;

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
