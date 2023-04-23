#include <scwx/qt/manager/update_manager.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace qt
{
namespace manager
{

TEST(UpdateManagerTest, CheckForUpdates)
{
   auto               updateManager = UpdateManager::Instance();
   bool               updateFound;
   types::gh::Release latestRelease;
   std::string        latestVersion;

   // Check for updates, and expect an update to be found
   updateFound   = updateManager->CheckForUpdates("0.0.0");
   latestRelease = updateManager->latest_release();
   latestVersion = updateManager->latest_version();

   EXPECT_EQ(updateFound, true);
   EXPECT_GT(latestRelease.name_.size(), 0);
   EXPECT_GT(latestRelease.htmlUrl_.size(), 0);
   EXPECT_GT(latestRelease.body_.size(), 0);
   EXPECT_EQ(latestRelease.draft_, false);
   EXPECT_EQ(latestRelease.prerelease_, false);
   EXPECT_GT(latestVersion, "0.0.0");

   // Check for updates, and expect no updates to be found
   updateFound = updateManager->CheckForUpdates("9999.99.99");

   EXPECT_EQ(updateFound, false);
}

} // namespace manager
} // namespace qt
} // namespace scwx
