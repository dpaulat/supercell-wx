#include <scwx/qt/map/map_perf.hpp>

#include <gtest/gtest.h>

#include <cstdlib>

namespace scwx::qt::map
{

class MapPerfTest : public ::testing::Test
{
protected:
   void SetUp() override { ResetMapPerfForTest(); }
};

TEST_F(MapPerfTest, disabled_by_default)
{
   EXPECT_FALSE(MapPerfEnabled());
   MapFramePerfSample sample {.paneId_ = 0, .totalMs_ = 1.0};
   RecordMapFramePerf(sample);
   EXPECT_EQ(MapPerfRecordedFrameCountForTest(), 0u);
}

TEST_F(MapPerfTest, records_when_env_enabled)
{
   setenv("SCWX_VULKAN_PERF", "1", 1);
   EXPECT_TRUE(MapPerfEnabled());

   MapFramePerfSample sample {
      .paneId_     = 2,
      .totalMs_    = 4.0,
      .mapLibreMs_ = 1.0,
      .overlayMs_  = 2.0,
   };
   RecordMapFramePerf(sample);
   EXPECT_EQ(MapPerfRecordedFrameCountForTest(), 1u);

   unsetenv("SCWX_VULKAN_PERF");
}

} // namespace scwx::qt::map
