#include <scwx/qt/settings/settings_variable.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace qt
{
namespace settings
{

TEST(SettingsVariableTest, Boolean)
{
   SettingsVariable<bool> boolVariable {"bool"};
   boolVariable.SetDefault(true);
   boolVariable.SetValue(false);

   EXPECT_EQ(boolVariable.name(), "bool");
   EXPECT_EQ(boolVariable.GetValue(), false);
   EXPECT_EQ(boolVariable.SetValue(true), true);
   EXPECT_EQ(boolVariable.GetValue(), true);
   EXPECT_EQ(boolVariable.SetValueOrDefault(false), true);
   EXPECT_EQ(boolVariable.GetValue(), false);
}

TEST(SettingsVariableTest, Double)
{
   SettingsVariable<double> doubleVariable {"double"};
   doubleVariable.SetDefault(4.2);
   doubleVariable.SetMinimum(1.0);
   doubleVariable.SetMaximum(9.9);
   doubleVariable.SetValue(5.0);

   EXPECT_EQ(doubleVariable.name(), "double");
   EXPECT_EQ(doubleVariable.GetValue(), 5.0);
   EXPECT_EQ(doubleVariable.SetValue(0), false);
   EXPECT_EQ(doubleVariable.GetValue(), 5.0);
   EXPECT_EQ(doubleVariable.SetValueOrDefault(0.0), false); // < Minimum
   EXPECT_EQ(doubleVariable.GetValue(), 1.0);
   EXPECT_EQ(doubleVariable.SetValueOrDefault(10.0), false); // > Maximum
   EXPECT_EQ(doubleVariable.GetValue(), 9.9);
   doubleVariable.SetValueToDefault();
   EXPECT_EQ(doubleVariable.GetValue(), 4.2);
   EXPECT_EQ(doubleVariable.SetValue(4.3), true);
   EXPECT_EQ(doubleVariable.GetValue(), 4.3);
   EXPECT_EQ(doubleVariable.SetValueOrDefault(5.7), true);
   EXPECT_EQ(doubleVariable.GetValue(), 5.7);

   EXPECT_EQ(doubleVariable.StageValue(0.0), false);
   EXPECT_EQ(doubleVariable.StageValue(5.0), true);
   EXPECT_EQ(doubleVariable.GetValue(), 5.7);
   doubleVariable.Commit();
   EXPECT_EQ(doubleVariable.GetValue(), 5.0);
}

TEST(SettingsVariableTest, Integer)
{
   SettingsVariable<int64_t> intVariable {"int64_t"};
   intVariable.SetDefault(42);
   intVariable.SetMinimum(10);
   intVariable.SetMaximum(99);
   intVariable.SetValue(50);

   EXPECT_EQ(intVariable.name(), "int64_t");
   EXPECT_EQ(intVariable.GetValue(), 50);
   EXPECT_EQ(intVariable.SetValue(0), false);
   EXPECT_EQ(intVariable.GetValue(), 50);
   EXPECT_EQ(intVariable.SetValueOrDefault(0), false); // < Minimum
   EXPECT_EQ(intVariable.GetValue(), 10);
   EXPECT_EQ(intVariable.SetValueOrDefault(100), false); // > Maximum
   EXPECT_EQ(intVariable.GetValue(), 99);
   intVariable.SetValueToDefault();
   EXPECT_EQ(intVariable.GetValue(), 42);
   EXPECT_EQ(intVariable.SetValue(43), true);
   EXPECT_EQ(intVariable.GetValue(), 43);
   EXPECT_EQ(intVariable.SetValueOrDefault(57), true);
   EXPECT_EQ(intVariable.GetValue(), 57);

   EXPECT_EQ(intVariable.StageValue(0), false);
   EXPECT_EQ(intVariable.StageValue(50), true);
   EXPECT_EQ(intVariable.GetValue(), 57);
   intVariable.Commit();
   EXPECT_EQ(intVariable.GetValue(), 50);
}

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
