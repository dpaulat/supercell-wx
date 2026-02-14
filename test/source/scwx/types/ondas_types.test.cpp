#include <scwx/types/ondas_types.hpp>

#include <gtest/gtest.h>

namespace scwx::types::ondas
{

TEST(OndasTypesTest, DirListWithoutSize)
{
   static const std::string kDirListContent = R"(
file1
file2
file3 spaces
)";

   std::vector<OndasDirListRecord> records = ParseOndasDirList(kDirListContent);

   ASSERT_EQ(records.size(), 3);
   EXPECT_EQ(records[0].filename_, "file1");
   EXPECT_EQ(records[0].size_, 0);
   EXPECT_EQ(records[1].filename_, "file2");
   EXPECT_EQ(records[1].size_, 0);
   EXPECT_EQ(records[2].filename_, "file3 spaces");
   EXPECT_EQ(records[2].size_, 0);
}

TEST(OndasTypesTest, DirListWithSize)
{
   static const std::string kDirListContent = R"(
12345 file1
67890 file2
54321 file3 spaces
)";

   std::vector<OndasDirListRecord> records = ParseOndasDirList(kDirListContent);

   ASSERT_EQ(records.size(), 3);
   EXPECT_EQ(records[0].filename_, "file1");
   EXPECT_EQ(records[0].size_, 12345);
   EXPECT_EQ(records[1].filename_, "file2");
   EXPECT_EQ(records[1].size_, 67890);
   EXPECT_EQ(records[2].filename_, "file3 spaces");
   EXPECT_EQ(records[2].size_, 54321);
}

} // namespace scwx::types::ondas
