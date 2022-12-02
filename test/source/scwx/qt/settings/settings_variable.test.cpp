#include <scwx/qt/settings/settings_variable.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace qt
{
namespace settings
{

TEST(SettingsVariableTest, String)
{
   SettingsVariable<std::string> stringVariable {"string"};
   stringVariable.SetDefault("Default");
   stringVariable.SetValidator([](const std::string& value)
                               { return !value.empty(); });
   stringVariable.SetValue("Hello World");

   EXPECT_EQ(stringVariable.name(), "string");
   EXPECT_EQ(stringVariable.GetValue(), "Hello World");
   EXPECT_EQ(stringVariable.SetValue(""), false);
   EXPECT_EQ(stringVariable.GetValue(), "Hello World");
   EXPECT_EQ(stringVariable.SetValueOrDefault(""), false);
   EXPECT_EQ(stringVariable.GetValue(), "Default");
   EXPECT_EQ(stringVariable.SetValue("Value 1"), true);
   EXPECT_EQ(stringVariable.GetValue(), "Value 1");
   EXPECT_EQ(stringVariable.SetValueOrDefault("Value 2"), true);
   EXPECT_EQ(stringVariable.GetValue(), "Value 2");
}

} // namespace settings
} // namespace qt
} // namespace scwx
