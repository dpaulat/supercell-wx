#include <scwx/gr/placefile.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace gr
{

TEST(PlacefileTest, OldExample)
{
   std::string filename(std::string(SCWX_TEST_DATA_DIR) +
                        "/gr/placefiles/placefile-old-example.txt");

   std::shared_ptr<Placefile> ct = Placefile::Load(filename);

   EXPECT_EQ(true, true);
}

} // namespace gr
} // namespace scwx
