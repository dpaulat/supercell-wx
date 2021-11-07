#include <scwx/util/streams.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace util
{

void VerifyTokens(const std::vector<std::string>& tokens)
{
   ASSERT_EQ(tokens.size(), 4);
   EXPECT_EQ(tokens[0], "One");
   EXPECT_EQ(tokens[1], "Two");
   EXPECT_EQ(tokens[2], "Three");
   EXPECT_EQ(tokens[3], "");
}

TEST(StreamsTest, CRNoEnd)
{
   std::stringstream        ss {"One\rTwo\rThree"};
   std::vector<std::string> tokens;
   std::string              t;

   while (scwx::util::getline(ss, t))
   {
      tokens.push_back(t);
   }

   EXPECT_EQ(ss.eof(), true);

   VerifyTokens(tokens);
}

TEST(StreamsTest, CRWithEnd)
{
   std::stringstream        ss {"One\rTwo\rThree\r"};
   std::vector<std::string> tokens;
   std::string              t;

   while (scwx::util::getline(ss, t))
   {
      tokens.push_back(t);
   }

   EXPECT_EQ(ss.eof(), true);

   VerifyTokens(tokens);
}

TEST(StreamsTest, CRLFNoEnd)
{
   std::stringstream        ss {"One\r\nTwo\r\nThree"};
   std::vector<std::string> tokens;
   std::string              t;

   while (scwx::util::getline(ss, t))
   {
      tokens.push_back(t);
   }

   EXPECT_EQ(ss.eof(), true);

   VerifyTokens(tokens);
}

TEST(StreamsTest, CRLFWithEnd)
{
   std::stringstream        ss {"One\r\nTwo\r\nThree\r\n"};
   std::vector<std::string> tokens;
   std::string              t;

   while (scwx::util::getline(ss, t))
   {
      tokens.push_back(t);
   }

   EXPECT_EQ(ss.eof(), true);

   VerifyTokens(tokens);
}

TEST(StreamsTest, LFNoEnd)
{
   std::stringstream        ss {"One\nTwo\nThree"};
   std::vector<std::string> tokens;
   std::string              t;

   while (scwx::util::getline(ss, t))
   {
      tokens.push_back(t);
   }

   EXPECT_EQ(ss.eof(), true);

   VerifyTokens(tokens);
}

TEST(StreamsTest, LFWithEnd)
{
   std::stringstream        ss {"One\nTwo\nThree\n"};
   std::vector<std::string> tokens;
   std::string              t;

   while (scwx::util::getline(ss, t))
   {
      tokens.push_back(t);
   }

   EXPECT_EQ(ss.eof(), true);

   VerifyTokens(tokens);
}

} // namespace util
} // namespace scwx
