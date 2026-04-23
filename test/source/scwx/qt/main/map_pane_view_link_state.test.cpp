#include <scwx/qt/main/map_pane_view_link_state.hpp>

#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using scwx::qt::main::SerializeMapPaneViewLinkStateJson;
using scwx::qt::main::TryParseMapPaneViewLinkStateJson;

TEST(MapPaneViewLinkState, roundtrip_2x2)
{
   const std::vector<bool> in {true, false, true, true};
   const std::string       ser = SerializeMapPaneViewLinkStateJson(2, 2, in);
   ASSERT_FALSE(ser.empty());
   const auto out = TryParseMapPaneViewLinkStateJson(ser, 2, 2);
   ASSERT_TRUE(out.has_value());
   ASSERT_EQ(out->size(), 4u);
   EXPECT_EQ((*out)[0], true);
   EXPECT_EQ((*out)[1], false);
   EXPECT_EQ((*out)[2], true);
   EXPECT_EQ((*out)[3], true);
}

TEST(MapPaneViewLinkState, grid_mismatch_rejected)
{
   const std::string ser =
      SerializeMapPaneViewLinkStateJson(2, 1, {true, true});
   ASSERT_FALSE(ser.empty());
   const auto w = TryParseMapPaneViewLinkStateJson(ser, 1, 2);
   EXPECT_FALSE(w.has_value());
}

TEST(MapPaneViewLinkState, numeric_zero_one_accepted)
{
   QJsonObject root;
   root["gw"] = 2.0;
   root["gh"] = 1.0;
   QJsonArray a;
   a.append(0);
   a.append(1);
   root["linked"] = a;
   const std::string json =
      QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString();
   const auto out = TryParseMapPaneViewLinkStateJson(json, 2, 1);
   ASSERT_TRUE(out.has_value());
   ASSERT_EQ(out->size(), 2u);
   EXPECT_FALSE(out->at(0));
   EXPECT_TRUE(out->at(1));
}

TEST(MapPaneViewLinkState, bool_strict_element_rejected)
{
   // Array with string "maybe" is invalid
   QJsonObject root;
   root["gw"] = 1.0;
   root["gh"] = 1.0;
   QJsonArray a;
   a.append(QStringLiteral("maybe"));
   root["linked"] = a;
   const std::string json =
      QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString();
   EXPECT_FALSE(TryParseMapPaneViewLinkStateJson(json, 1, 1).has_value());
}

TEST(MapPaneViewLinkState, serialize_rejects_size_mismatch)
{
   const std::string s = SerializeMapPaneViewLinkStateJson(2, 2, {true, false});
   EXPECT_TRUE(s.empty());
}
