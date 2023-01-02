#include <scwx/qt/util/q_file_input_stream.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace qt
{
namespace util
{
static const std::string kLoremIpsum_ {std::string(SCWX_TEST_DATA_DIR) +
                                       "/text/lorem-ipsum.txt"};

TEST(QFileInputStream, Get)
{
   QFileInputStream is {kLoremIpsum_};

   EXPECT_EQ(is.is_open(), true);
   EXPECT_EQ(is.good(), true);

   std::string s;

   for (size_t i = 0; i < 5; ++i)
   {
      int c = is.get();
      EXPECT_NE(c, EOF);
      EXPECT_EQ(is.good(), true);
      s.push_back(static_cast<char>(c));
   }

   EXPECT_EQ(s, "Lorem");
}

TEST(QFileInputStream, GetLine)
{
   using namespace std::string_literals;

   QFileInputStream is {kLoremIpsum_};

   EXPECT_EQ(is.is_open(), true);
   EXPECT_EQ(is.good(), true);

   std::string s;

   s.resize(5u);
   is.getline(s.data(), 5u);

   EXPECT_EQ(is.good(), false);
   EXPECT_EQ(is.fail(), true);
   EXPECT_EQ(s, "Lore\0"s);
}

TEST(QFileInputStream, Read)
{
   QFileInputStream is {kLoremIpsum_};

   EXPECT_EQ(is.is_open(), true);
   EXPECT_EQ(is.good(), true);

   std::string s;

   s.resize(5u);
   is.read(s.data(), 5u);

   EXPECT_EQ(is.good(), true);
   EXPECT_EQ(s, "Lorem");
}

TEST(QFileInputStream, SeekOffset)
{
   QFileInputStream is {kLoremIpsum_};

   EXPECT_EQ(is.is_open(), true);
   EXPECT_EQ(is.good(), true);

   is.seekg(6, std::ios_base::beg);

   std::string s;
   s.resize(5u);
   is.read(s.data(), 5u);
   EXPECT_EQ(is.good(), true);
   EXPECT_EQ(s, "ipsum");

   is.seekg(1, std::ios_base::cur);

   is.read(s.data(), 5u);
   EXPECT_EQ(is.good(), true);
   EXPECT_EQ(s, "dolor");

   is.seekg(-8, std::ios_base::end);

   is.read(s.data(), 5u);
   EXPECT_EQ(is.good(), true);
   EXPECT_EQ(s, "labor");
}

} // namespace util
} // namespace qt
} // namespace scwx
