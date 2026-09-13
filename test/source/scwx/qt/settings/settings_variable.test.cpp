#include <scwx/qt/settings/settings_variable.hpp>

#include <boost/json.hpp>
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

TEST(SettingsVariableTest, ReadValueInvalidTypeUsesDefault)
{
   SettingsVariable<bool> boolVariable {"bool"};
   boolVariable.SetDefault(true);
   boolVariable.SetValue(false);

   const boost::json::object stringValue {{"bool", ""}};
   EXPECT_FALSE(boolVariable.ReadValue(stringValue));
   EXPECT_EQ(boolVariable.GetValue(), true);

   boolVariable.SetValue(false);
   const boost::json::object integerValue {{"bool", 0}};
   EXPECT_FALSE(boolVariable.ReadValue(integerValue));
   EXPECT_EQ(boolVariable.GetValue(), true);

   const boost::json::object validValue {{"bool", false}};
   EXPECT_TRUE(boolVariable.ReadValue(validValue));
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

TEST(SettingsVariableTest, ChangedEventSetValue)
{
   SettingsVariable<int64_t> intVariable {"int64_t"};
   intVariable.SetDefault(10);
   intVariable.SetValue(20);

   std::optional<ChangedEvent<int64_t>> lastChangedEvent;
   std::optional<ChangedEvent<int64_t>> lastStagedEvent;

   intVariable.changed_signal().connect([&](const ChangedEvent<int64_t>& event)
                                        { lastChangedEvent = event; });
   intVariable.staged_signal().connect([&](const ChangedEvent<int64_t>& event)
                                       { lastStagedEvent = event; });

   ASSERT_TRUE(intVariable.SetValue(30));

   ASSERT_TRUE(lastChangedEvent.has_value());
   EXPECT_EQ(lastChangedEvent->oldValue_, 20);
   EXPECT_EQ(lastChangedEvent->newValue_, 30);

   ASSERT_TRUE(lastStagedEvent.has_value());
   EXPECT_EQ(lastStagedEvent->oldValue_, 20);
   EXPECT_EQ(lastStagedEvent->newValue_, 30);

   // Verify no signal is emitted on invalid SetValue
   lastChangedEvent.reset();
   lastStagedEvent.reset();
   intVariable.SetMinimum(5);
   ASSERT_FALSE(intVariable.SetValue(1));
   EXPECT_FALSE(lastChangedEvent.has_value());
   EXPECT_FALSE(lastStagedEvent.has_value());
}

TEST(SettingsVariableTest, ChangedEventSetValueToDefault)
{
   SettingsVariable<int64_t> intVariable {"int64_t"};
   intVariable.SetDefault(10);
   intVariable.SetValue(20);

   std::optional<ChangedEvent<int64_t>> lastChangedEvent;
   std::optional<ChangedEvent<int64_t>> lastStagedEvent;

   intVariable.changed_signal().connect([&](const ChangedEvent<int64_t>& event)
                                        { lastChangedEvent = event; });
   intVariable.staged_signal().connect([&](const ChangedEvent<int64_t>& event)
                                       { lastStagedEvent = event; });

   intVariable.SetValueToDefault();

   ASSERT_TRUE(lastChangedEvent.has_value());
   EXPECT_EQ(lastChangedEvent->oldValue_, 20);
   EXPECT_EQ(lastChangedEvent->newValue_, 10);

   ASSERT_TRUE(lastStagedEvent.has_value());
   EXPECT_EQ(lastStagedEvent->oldValue_, 20);
   EXPECT_EQ(lastStagedEvent->newValue_, 10);
}

TEST(SettingsVariableTest, ChangedEventStageAndCommit)
{
   SettingsVariable<int64_t> intVariable {"int64_t"};
   intVariable.SetDefault(10);
   intVariable.SetValue(20);

   std::optional<ChangedEvent<int64_t>> lastChangedEvent;
   std::optional<ChangedEvent<int64_t>> lastStagedEvent;

   intVariable.changed_signal().connect([&](const ChangedEvent<int64_t>& event)
                                        { lastChangedEvent = event; });
   intVariable.staged_signal().connect([&](const ChangedEvent<int64_t>& event)
                                       { lastStagedEvent = event; });

   // StageValue should only fire staged_signal, not changed_signal
   ASSERT_TRUE(intVariable.StageValue(30));
   EXPECT_FALSE(lastChangedEvent.has_value());
   ASSERT_TRUE(lastStagedEvent.has_value());
   EXPECT_EQ(lastStagedEvent->oldValue_, 20);
   EXPECT_EQ(lastStagedEvent->newValue_, 30);

   // Commit should fire both signals
   lastStagedEvent.reset();
   ASSERT_TRUE(intVariable.Commit());

   ASSERT_TRUE(lastChangedEvent.has_value());
   EXPECT_EQ(lastChangedEvent->oldValue_, 20);
   EXPECT_EQ(lastChangedEvent->newValue_, 30);

   // The staged signal reports the old staged value (30) and the new current
   // value (30) after commit, reflecting that no further staged change remains.
   ASSERT_TRUE(lastStagedEvent.has_value());
   EXPECT_EQ(lastStagedEvent->oldValue_, 30);
   EXPECT_EQ(lastStagedEvent->newValue_, 30);
}

} // namespace settings
} // namespace qt
} // namespace scwx
