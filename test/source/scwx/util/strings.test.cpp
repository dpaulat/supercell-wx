#include <scwx/util/strings.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace util
{

class BytesToStringTest :
    public testing::TestWithParam<std::pair<std::ptrdiff_t, std::string>>
{
};

TEST_P(BytesToStringTest, BytesToString)
{
   auto& [bytes, expected] = GetParam();

   std::string s = BytesToString(bytes);

   EXPECT_EQ(s, expected);
}

INSTANTIATE_TEST_SUITE_P(StringsTest,
                         BytesToStringTest,
                         testing::Values(std::make_pair(123, "123 bytes"),
                                         std::make_pair(1000, "0.98 KB"),
                                         std::make_pair(1018, "0.99 KB"),
                                         std::make_pair(1024, "1.0 KB"),
                                         std::make_pair(1127, "1.1 KB"),
                                         std::make_pair(1260, "1.23 KB"),
                                         std::make_pair(24012, "23.4 KB"),
                                         std::make_pair(353974, "346 KB"),
                                         std::make_pair(1024000, "0.98 MB"),
                                         std::make_pair(1048576000, "0.98 GB"),
                                         std::make_pair(1073741824000,
                                                        "0.98 TB")));

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
