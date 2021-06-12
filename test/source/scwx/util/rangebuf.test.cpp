#include <scwx/util/rangebuf.hpp>

#include <gtest/gtest.h>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>

namespace scwx
{
namespace util
{

TEST(rangebuf, smiles_mile)
{
   std::istringstream iss("smiles");
   iss.seekg(1);
   util::rangebuf rb(iss.rdbuf(), 4);

   std::ostringstream oss;

   boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
   in.push(rb);

   std::streamsize bytesCopied = boost::iostreams::copy(in, oss);
   std::string     substring {oss.str()};

   EXPECT_EQ(substring, "mile");
   EXPECT_EQ(bytesCopied, 4);
   EXPECT_EQ(iss.tellg(), 5);
   EXPECT_EQ(iss.eof(), false);
}

} // namespace util
} // namespace scwx
