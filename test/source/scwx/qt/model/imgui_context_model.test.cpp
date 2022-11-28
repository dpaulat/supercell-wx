#include <scwx/qt/model/imgui_context_model.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace qt
{
namespace model
{

TEST(ImGuiContextModelTest, State)
{
   auto& imGuiContextModel = ImGuiContextModel::Instance();

   imGuiContextModel.CreateContext("Context One");
   imGuiContextModel.CreateContext("Context Two");
   imGuiContextModel.CreateContext("Context Three");

   auto contexts = imGuiContextModel.contexts();

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

   imGuiContextModel.DestroyContext("Context Two");

   auto contexts2 = imGuiContextModel.contexts();

   ASSERT_EQ(contexts2.size(), 2u);
   EXPECT_EQ(contexts2.at(0), contexts.at(0));
   EXPECT_EQ(contexts2.at(1), contexts.at(2));
}

} // namespace model
} // namespace qt
} // namespace scwx
