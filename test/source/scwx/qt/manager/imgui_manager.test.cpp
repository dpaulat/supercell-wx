#include <scwx/qt/manager/imgui_manager.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace qt
{
namespace manager
{

TEST(ImGuiManagerTest, State)
{
   auto& ImGuiManager = ImGuiManager::Instance();

   ImGuiManager.CreateContext("Context One");
   ImGuiManager.CreateContext("Context Two");
   ImGuiManager.CreateContext("Context Three");
   
   auto contexts = ImGuiManager.contexts();

   ASSERT_EQ(contexts.size(), 3u);
   EXPECT_EQ(contexts.at(0).id_, 0u);
   EXPECT_EQ(contexts.at(1).id_, 1u);
   EXPECT_EQ(contexts.at(2).id_, 2u);
   EXPECT_EQ(contexts.at(0).name_, "Context One");
   EXPECT_EQ(contexts.at(1).name_, "Context Two");
   EXPECT_EQ(contexts.at(2).name_, "Context Three");
   EXPECT_NE(contexts.at(0).context_, nullptr);
   EXPECT_NE(contexts.at(1).context_, nullptr);
   EXPECT_NE(contexts.at(2).context_, nullptr);

   ImGuiManager.DestroyContext("Context Two");

   auto contexts2 = ImGuiManager.contexts();

   ASSERT_EQ(contexts2.size(), 2u);
   EXPECT_EQ(contexts2.at(0), contexts.at(0));
   EXPECT_EQ(contexts2.at(1), contexts.at(2));
}

} // namespace manager
} // namespace qt
} // namespace scwx
