#include <scwx/util/strings.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace util
{

TEST(StringsTest, ParseTokensColor)
{
   static const std::string line {"Color: red green blue alpha discarded"};
   static const std::vector<std::string> delimiters {":", " ", " ", " ", " "};

   std::vector<std::string> tokens = ParseTokens(line, delimiters);

   ASSERT_EQ(tokens.size(), 6);
   EXPECT_EQ(tokens[0], "Color");
   EXPECT_EQ(tokens[1], "red");
   EXPECT_EQ(tokens[2], "green");
   EXPECT_EQ(tokens[3], "blue");
   EXPECT_EQ(tokens[4], "alpha");
   EXPECT_EQ(tokens[5], "discarded");
}

TEST(StringsTest, ParseTokensColorOffset)
{
   static const std::string              line {"Color: red green blue alpha"};
   static const std::vector<std::string> delimiters {" ", " ", " ", " "};
   static const std::size_t              offset = std::string {"Color:"}.size();

   std::vector<std::string> tokens = ParseTokens(line, delimiters, offset);

   ASSERT_EQ(tokens.size(), 4);
   EXPECT_EQ(tokens[0], "red");
   EXPECT_EQ(tokens[1], "green");
   EXPECT_EQ(tokens[2], "blue");
   EXPECT_EQ(tokens[3], "alpha");
}

TEST(StringsTest, ParseTokensText)
{
   static const std::string line {
      "Text: lat, lon, fontNumber, \"string, string\", \"hover, hover\", "
      "discarded"};
   static const std::vector<std::string> delimiters {
      ":", ",", ",", ",", ",", ","};

   std::vector<std::string> tokens = ParseTokens(line, delimiters);

   ASSERT_EQ(tokens.size(), 7);
   EXPECT_EQ(tokens[0], "Text");
   EXPECT_EQ(tokens[1], "lat");
   EXPECT_EQ(tokens[2], "lon");
   EXPECT_EQ(tokens[3], "fontNumber");
   EXPECT_EQ(tokens[4], "\"string, string\"");
   EXPECT_EQ(tokens[5], "\"hover, hover\"");
   EXPECT_EQ(tokens[6], "discarded");
}

} // namespace util
} // namespace scwx
