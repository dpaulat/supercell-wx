#include <scwx/awips/coded_time_motion_location.hpp>

#include <gtest/gtest.h>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace awips
{

static const std::string logPrefix_ =
   "[scwx::awips::coded_time_motion_location.test] ";

TEST(CodedTimeMotionLocation, LeadingZeroes)
{
   using namespace std::chrono;

   std::vector<std::string> data = {
      "TIME...MOT...LOC 0128Z 004DEG 9KT 3480 10318"};

   CodedTimeMotionLocation tml;
   bool                    dataValid = tml.Parse(data);

   ASSERT_EQ(dataValid, true);

   EXPECT_EQ(tml.time().to_duration(), 1h + 28min);
   EXPECT_EQ(tml.direction(), 4);
   EXPECT_EQ(tml.speed(), 9);

   auto coordinates = tml.coordinates();

   ASSERT_EQ(coordinates.size(), 1);

   EXPECT_DOUBLE_EQ(coordinates[0].latitude_, 34.80);
   EXPECT_DOUBLE_EQ(coordinates[0].longitude_, -103.18);
}

TEST(CodedTimeMotionLocation, Stationary)
{
   using namespace std::chrono;

   std::vector<std::string> data = {
      "TIME...MOT...LOC 1959Z 254DEG 0KT 3253 11464"};

   CodedTimeMotionLocation tml;
   bool                    dataValid = tml.Parse(data);

   ASSERT_EQ(dataValid, true);

   EXPECT_EQ(tml.time().to_duration(), 19h + 59min);
   EXPECT_EQ(tml.direction(), 254);
   EXPECT_EQ(tml.speed(), 0);

   auto coordinates = tml.coordinates();

   ASSERT_EQ(coordinates.size(), 1);

   EXPECT_DOUBLE_EQ(coordinates[0].latitude_, 32.53);
   EXPECT_DOUBLE_EQ(coordinates[0].longitude_, -114.64);
}

TEST(CodedTimeMotionLocation, TwoCoordinates)
{
   using namespace std::chrono;

   std::vector<std::string> data = {
      "TIME...MOT...LOC 2113Z 345DEG 42KT 2760 8211 2724 8198"};

   CodedTimeMotionLocation tml;
   bool                    dataValid = tml.Parse(data);

   ASSERT_EQ(dataValid, true);

   EXPECT_EQ(tml.time().to_duration(), 21h + 13min);
   EXPECT_EQ(tml.direction(), 345);
   EXPECT_EQ(tml.speed(), 42);

   auto coordinates = tml.coordinates();

   ASSERT_EQ(coordinates.size(), 2);

   EXPECT_DOUBLE_EQ(coordinates[0].latitude_, 27.6);
   EXPECT_DOUBLE_EQ(coordinates[0].longitude_, -82.11);
   EXPECT_DOUBLE_EQ(coordinates[1].latitude_, 27.24);
   EXPECT_DOUBLE_EQ(coordinates[1].longitude_, -81.98);
}

TEST(CodedTimeMotionLocation, EmptyData)
{
   std::vector<std::string> data = {};

   CodedTimeMotionLocation tml;
   bool                    dataValid = tml.Parse(data);

   EXPECT_EQ(dataValid, false);
}

TEST(CodedTimeMotionLocation, MalformedData)
{
   std::vector<std::string> data = {"TIME...MOT...LOC 2113Z 345DEG 42KT 2760"};

   CodedTimeMotionLocation tml;
   bool                    dataValid = tml.Parse(data);

   EXPECT_EQ(dataValid, false);
}

} // namespace awips
} // namespace scwx
