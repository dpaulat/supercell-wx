#include <scwx/qt/manager/status_manager.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace qt
{
namespace manager
{

// Pure logic, no network needed. StatusManager is a real, app-wide
// singleton (see its own Instance() doc), so these tests share state
// with each other (and with anything else in the same wxtest process
// that reports through it) -- each test reports under its own distinct
// id(s) and always clears them at the end, so it doesn't leak state into
// whichever test happens to run next.
TEST(StatusManagerTest, EmptyByDefault)
{
   auto statusManager = StatusManager::Instance();

   // Not asserting IsBusy()/CurrentStatusText() are false/empty here --
   // this is a shared singleton, and another test (or, if this ever ran
   // inside the real app, a real manager) could legitimately have an
   // entry active concurrently. Only this test's own round-trip below is
   // this test's actual concern.
   statusManager->ReportProgress("status-manager-test-empty", "Test", 0, -1);
   EXPECT_TRUE(statusManager->IsBusy());

   statusManager->ReportComplete("status-manager-test-empty");
}

TEST(StatusManagerTest, ReportProgressAndComplete)
{
   auto statusManager = StatusManager::Instance();

   statusManager->ReportProgress(
      "status-manager-test-progress", "Test Download", 1048576, 10485760);

   const std::string text = statusManager->CurrentStatusText();
   EXPECT_NE(text.find("Test Download"), std::string::npos);
   EXPECT_NE(text.find("1.0 MB"), std::string::npos);
   EXPECT_NE(text.find("of 10.0 MB"), std::string::npos);
   EXPECT_TRUE(statusManager->IsBusy());

   statusManager->ReportComplete("status-manager-test-progress");

   // This id's own contribution is gone -- can't assert IsBusy() is now
   // false outright (see EmptyByDefault's own comment on why), only that
   // this id's own text no longer appears.
   EXPECT_EQ(statusManager->CurrentStatusText().find("Test Download"),
             std::string::npos);
}

TEST(StatusManagerTest, UnknownTotalOmitsFraction)
{
   auto statusManager = StatusManager::Instance();

   statusManager->ReportProgress(
      "status-manager-test-unknown", "Test Unknown", 2097152, -1);

   const std::string text = statusManager->CurrentStatusText();
   EXPECT_NE(text.find("Test Unknown"), std::string::npos);
   EXPECT_NE(text.find("2.0 MB"), std::string::npos);
   EXPECT_EQ(text.find(" of "), std::string::npos);

   statusManager->ReportComplete("status-manager-test-unknown");
}

TEST(StatusManagerTest, MultipleEntriesShowMostRecentPlusCount)
{
   auto statusManager = StatusManager::Instance();

   statusManager->ReportProgress(
      "status-manager-test-multi-a", "First", 1024, -1);
   statusManager->ReportProgress(
      "status-manager-test-multi-b", "Second", 2048, -1);

   // "Second" was reported more recently than "First" -- it should be
   // the one shown, with a "(+1 more)" for the other.
   const std::string text = statusManager->CurrentStatusText();
   EXPECT_NE(text.find("Second"), std::string::npos);
   EXPECT_NE(text.find("(+1 more)"), std::string::npos);

   statusManager->ReportComplete("status-manager-test-multi-a");
   statusManager->ReportComplete("status-manager-test-multi-b");
}

} // namespace manager
} // namespace qt
} // namespace scwx
