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
file4_20260705_1100
20260705_1200
20260705_1300 spaces
)";

   std::vector<OndasDirListRecord> records = ParseOndasDirList(kDirListContent);

   ASSERT_EQ(records.size(), 6);
   EXPECT_EQ(records[0].filename_, "file1");
   EXPECT_EQ(records[0].size_, 0);
   EXPECT_EQ(records[1].filename_, "file2");
   EXPECT_EQ(records[1].size_, 0);
   EXPECT_EQ(records[2].filename_, "file3 spaces");
   EXPECT_EQ(records[2].size_, 0);
   EXPECT_EQ(records[3].filename_, "file4_20260705_1100");
   EXPECT_EQ(records[3].size_, 0);
   EXPECT_EQ(records[4].filename_, "20260705_1200");
   EXPECT_EQ(records[4].size_, 0);
   EXPECT_EQ(records[5].filename_, "20260705_1300 spaces");
   EXPECT_EQ(records[5].size_, 0);
}

TEST(OndasTypesTest, DirListWithSize)
{
   static const std::string kDirListContent = R"(
12345 file1
67890 file2
54321 file3 spaces
98765 file4_20260705_1100
74185 20260705_1200
96396 20260705_1300 spaces
)";

   std::vector<OndasDirListRecord> records = ParseOndasDirList(kDirListContent);

   ASSERT_EQ(records.size(), 6);
   EXPECT_EQ(records[0].filename_, "file1");
   EXPECT_EQ(records[0].size_, 12345);
   EXPECT_EQ(records[1].filename_, "file2");
   EXPECT_EQ(records[1].size_, 67890);
   EXPECT_EQ(records[2].filename_, "file3 spaces");
   EXPECT_EQ(records[2].size_, 54321);
   EXPECT_EQ(records[3].filename_, "file4_20260705_1100");
   EXPECT_EQ(records[3].size_, 98765);
   EXPECT_EQ(records[4].filename_, "20260705_1200");
   EXPECT_EQ(records[4].size_, 74185);
   EXPECT_EQ(records[5].filename_, "20260705_1300 spaces");
   EXPECT_EQ(records[5].size_, 96396);
}

} // namespace scwx::types::ondas
