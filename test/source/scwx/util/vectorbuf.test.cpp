#include <scwx/util/vectorbuf.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace util
{

TEST(vectorbuf, smiles)
{
   std::vector<char> v;
   vectorbuf         vb(v);
   std::istream      is(&vb);

   v.reserve(7);
   memcpy(v.data(), "smiles", 7);
   vb.update_read_pointers(7);

   EXPECT_EQ(is.eof(), false);
   EXPECT_EQ(is.fail(), false);

   char data[7];
   is.read(data, 7);

   EXPECT_EQ(std::string(data), std::string("smiles"));
   EXPECT_EQ(is.eof(), false);
   EXPECT_EQ(is.fail(), false);

   is.read(data, 1);

   EXPECT_EQ(is.eof(), true);
   EXPECT_EQ(is.fail(), true);
}

} // namespace util
} // namespace scwx
