#include <scwx/util/vectorbuf.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace util
{

class vectorbuf_test : public ::testing::Test
{
protected:
   vectorbuf_test() : v_(), vb_(v_), is_(&vb_), Test() {}
   ~vectorbuf_test() = default;

   void SetUp() override
   {
      v_.reserve(7);
      memcpy(v_.data(), "smiles", 7);
      vb_.update_read_pointers(7);
   }

   std::vector<char> v_;
   vectorbuf         vb_;
   std::istream      is_;
};

TEST_F(vectorbuf_test, smiles)
{
   EXPECT_EQ(is_.eof(), false);
   EXPECT_EQ(is_.fail(), false);

   char data[7];
   is_.read(data, 7);

   EXPECT_EQ(std::string(data), std::string("smiles"));
   EXPECT_EQ(is_.eof(), false);
   EXPECT_EQ(is_.fail(), false);

   is_.read(data, 1);

   EXPECT_EQ(is_.eof(), true);
   EXPECT_EQ(is_.fail(), true);
}

TEST_F(vectorbuf_test, seekg_begin)
{
   is_.seekg(1, std::ios_base::beg);

   char data[7] = {0};
   is_.read(data, 6);

   EXPECT_EQ(std::string(data), std::string("miles"));
   EXPECT_EQ(is_.eof(), false);
   EXPECT_EQ(is_.fail(), false);
}

TEST_F(vectorbuf_test, seekg_cur)
{
   char data[4] = {0};
   is_.read(data, 1);
   is_.seekg(2, std::ios_base::cur);
   is_.read(data, 3);

   EXPECT_EQ(std::string(data), std::string("les"));
   EXPECT_EQ(is_.eof(), false);
   EXPECT_EQ(is_.fail(), false);
}

TEST_F(vectorbuf_test, seekg_end)
{
   is_.seekg(-4, std::ios_base::end);

   char data[4] = {0};
   is_.read(data, 3);

   EXPECT_EQ(std::string(data), std::string("les"));
   EXPECT_EQ(is_.eof(), false);
   EXPECT_EQ(is_.fail(), false);
}

} // namespace util
} // namespace scwx
