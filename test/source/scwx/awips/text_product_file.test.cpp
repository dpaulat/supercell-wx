#include <scwx/awips/text_product_file.hpp>

#include <gtest/gtest.h>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace awips
{

static const std::string logPrefix_ = "[scwx::awips::text_product_file.test] ";

class TextProductValidFileTest : public testing::TestWithParam<std::string>
{
};

TEST_P(TextProductValidFileTest, ValidFile)
{
   TextProductFile file;

   auto param = GetParam();

   const std::string filename {std::string(SCWX_TEST_DATA_DIR) + param};
   file.LoadFile(filename);

   EXPECT_GT(file.message_count(), 0);
}

INSTANTIATE_TEST_SUITE_P(
   TextProductFile,
   TextProductValidFileTest,
   testing::Values("/warnings/warnings_20210604_21.txt",
                   "/warnings/warnings_20210606_15.txt",
                   "/warnings/warnings_20210606_22-59.txt",
                   "/nexrad/level3/KLSX_NOUS63_FTMLSX_202201041404"));

} // namespace awips
} // namespace scwx
