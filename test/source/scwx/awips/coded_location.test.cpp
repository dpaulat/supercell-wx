#include <scwx/awips/coded_location.hpp>

#include <gtest/gtest.h>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace awips
{

static const std::string logPrefix_ = "[scwx::awips::coded_location.test] ";

TEST(CodedLocation, WFO100W)
{
   std::vector<std::string> data = {
      "LAT...LON 4896 10015 4789 10017 4787 9995 4842 9987",
      "      4842 9955 4897 9958"};

   CodedLocation location;
   bool          dataValid = location.Parse(data);

   ASSERT_EQ(dataValid, true);

   auto coordinates = location.coordinates();

   ASSERT_EQ(coordinates.size(), 6);

   EXPECT_DOUBLE_EQ(coordinates[0].latitude_, 48.96);
   EXPECT_DOUBLE_EQ(coordinates[0].longitude_, -100.15);
   EXPECT_DOUBLE_EQ(coordinates[1].latitude_, 47.89);
   EXPECT_DOUBLE_EQ(coordinates[1].longitude_, -100.17);
   EXPECT_DOUBLE_EQ(coordinates[2].latitude_, 47.87);
   EXPECT_DOUBLE_EQ(coordinates[2].longitude_, -99.95);
   EXPECT_DOUBLE_EQ(coordinates[3].latitude_, 48.42);
   EXPECT_DOUBLE_EQ(coordinates[3].longitude_, -99.87);
   EXPECT_DOUBLE_EQ(coordinates[4].latitude_, 48.42);
   EXPECT_DOUBLE_EQ(coordinates[4].longitude_, -99.55);
   EXPECT_DOUBLE_EQ(coordinates[5].latitude_, 48.97);
   EXPECT_DOUBLE_EQ(coordinates[5].longitude_, -99.58);
}

TEST(CodedLocation, WFOWestWrap)
{
   std::vector<std::string> data = {
      "LAT...LON 4896 10015 4789 18100 4787 9995 4842 9987",
      "      4842 9955 4897 9958"};

   CodedLocation location;
   bool          dataValid = location.Parse(data);

   ASSERT_EQ(dataValid, true);

   auto coordinates = location.coordinates();

   ASSERT_EQ(coordinates.size(), 6);

   EXPECT_DOUBLE_EQ(coordinates[0].latitude_, 48.96);
   EXPECT_DOUBLE_EQ(coordinates[0].longitude_, -100.15);
   EXPECT_DOUBLE_EQ(coordinates[1].latitude_, 47.89);
   EXPECT_DOUBLE_EQ(coordinates[1].longitude_, 179.00);
   EXPECT_DOUBLE_EQ(coordinates[2].latitude_, 47.87);
   EXPECT_DOUBLE_EQ(coordinates[2].longitude_, -99.95);
   EXPECT_DOUBLE_EQ(coordinates[3].latitude_, 48.42);
   EXPECT_DOUBLE_EQ(coordinates[3].longitude_, -99.87);
   EXPECT_DOUBLE_EQ(coordinates[4].latitude_, 48.42);
   EXPECT_DOUBLE_EQ(coordinates[4].longitude_, -99.55);
   EXPECT_DOUBLE_EQ(coordinates[5].latitude_, 48.97);
   EXPECT_DOUBLE_EQ(coordinates[5].longitude_, -99.58);
}

TEST(CodedLocation, WFOGuam)
{
   std::vector<std::string> data = {
      "LAT...LON 1360 14509 1371 14495 1348 14463 1325 14492"};

   CodedLocation location;
   bool          dataValid = location.Parse(data, "PGUM");

   ASSERT_EQ(dataValid, true);

   auto coordinates = location.coordinates();

   ASSERT_EQ(coordinates.size(), 4);

   EXPECT_DOUBLE_EQ(coordinates[0].latitude_, 13.60);
   EXPECT_DOUBLE_EQ(coordinates[0].longitude_, 145.09);
   EXPECT_DOUBLE_EQ(coordinates[1].latitude_, 13.71);
   EXPECT_DOUBLE_EQ(coordinates[1].longitude_, 144.95);
   EXPECT_DOUBLE_EQ(coordinates[2].latitude_, 13.48);
   EXPECT_DOUBLE_EQ(coordinates[2].longitude_, 144.63);
   EXPECT_DOUBLE_EQ(coordinates[3].latitude_, 13.25);
   EXPECT_DOUBLE_EQ(coordinates[3].longitude_, 144.92);
}

TEST(CodedLocation, WFOGuamWrap)
{
   std::vector<std::string> data = {
      "LAT...LON 1360 14509 1371 18195 1348 14463 1325 14492"};

   CodedLocation location;
   bool          dataValid = location.Parse(data, "PGUM");

   ASSERT_EQ(dataValid, true);

   auto coordinates = location.coordinates();

   ASSERT_EQ(coordinates.size(), 4);

   EXPECT_DOUBLE_EQ(coordinates[0].latitude_, 13.60);
   EXPECT_DOUBLE_EQ(coordinates[0].longitude_, -145.09);
   EXPECT_DOUBLE_EQ(coordinates[1].latitude_, 13.71);
   EXPECT_DOUBLE_EQ(coordinates[1].longitude_, 178.05);
   EXPECT_DOUBLE_EQ(coordinates[2].latitude_, 13.48);
   EXPECT_DOUBLE_EQ(coordinates[2].longitude_, -144.63);
   EXPECT_DOUBLE_EQ(coordinates[3].latitude_, 13.25);
   EXPECT_DOUBLE_EQ(coordinates[3].longitude_, -144.92);
}

TEST(CodedLocation, NC100W)
{
   std::vector<std::string> data = {
      "LAT...LON 46680254 49089563 47069563 44650254"};

   CodedLocation location;
   bool          dataValid = location.Parse(data);

   ASSERT_EQ(dataValid, true);

   auto coordinates = location.coordinates();

   ASSERT_EQ(coordinates.size(), 4);

   EXPECT_DOUBLE_EQ(coordinates[0].latitude_, 46.68);
   EXPECT_DOUBLE_EQ(coordinates[0].longitude_, -102.54);
   EXPECT_DOUBLE_EQ(coordinates[1].latitude_, 49.08);
   EXPECT_DOUBLE_EQ(coordinates[1].longitude_, -95.63);
   EXPECT_DOUBLE_EQ(coordinates[2].latitude_, 47.06);
   EXPECT_DOUBLE_EQ(coordinates[2].longitude_, -95.63);
   EXPECT_DOUBLE_EQ(coordinates[3].latitude_, 44.65);
   EXPECT_DOUBLE_EQ(coordinates[3].longitude_, -102.54);
}

TEST(CodedLocation, NCWashington)
{
   std::vector<std::string> data = {
      "LAT...LON 49112272 49092189 48662129 48022117 47452115",
      "      47072129 46622149 46502228 46292349 46442456",
      "      46722439 47802499 48522503 48422448 48252386",
      "      48332313"};

   CodedLocation location;

   bool dataValid = location.Parse(data);

   ASSERT_EQ(dataValid, true);

   auto coordinates = location.coordinates();

   ASSERT_EQ(coordinates.size(), 16);

   EXPECT_DOUBLE_EQ(coordinates[0].latitude_, 49.11);
   EXPECT_DOUBLE_EQ(coordinates[0].longitude_, -122.72);
   EXPECT_DOUBLE_EQ(coordinates[1].latitude_, 49.09);
   EXPECT_DOUBLE_EQ(coordinates[1].longitude_, -121.89);
   EXPECT_DOUBLE_EQ(coordinates[14].latitude_, 48.25);
   EXPECT_DOUBLE_EQ(coordinates[14].longitude_, -123.86);
   EXPECT_DOUBLE_EQ(coordinates[15].latitude_, 48.33);
   EXPECT_DOUBLE_EQ(coordinates[15].longitude_, -123.13);
}

TEST(CodedLocation, NCMaine)
{
   std::vector<std::string> data = {
      "LAT...LON 47316870 47216795 46466767 45436766 44756779",
      "      44216834 43816943 43706970 43837006 44497009",
      "      45306974 46356946 46976921"};

   CodedLocation location;
   bool          dataValid = location.Parse(data);

   ASSERT_EQ(dataValid, true);

   auto coordinates = location.coordinates();

   ASSERT_EQ(coordinates.size(), 13);

   EXPECT_DOUBLE_EQ(coordinates[0].latitude_, 47.31);
   EXPECT_DOUBLE_EQ(coordinates[0].longitude_, -68.70);
   EXPECT_DOUBLE_EQ(coordinates[1].latitude_, 47.21);
   EXPECT_DOUBLE_EQ(coordinates[1].longitude_, -67.95);
   EXPECT_DOUBLE_EQ(coordinates[11].latitude_, 46.35);
   EXPECT_DOUBLE_EQ(coordinates[11].longitude_, -69.46);
   EXPECT_DOUBLE_EQ(coordinates[12].latitude_, 46.97);
   EXPECT_DOUBLE_EQ(coordinates[12].longitude_, -69.21);
}

TEST(CodedLocation, InvalidNC)
{
   std::vector<std::string> data = {
      "LAT...LON 47316870 4721679 46466767 45436766 44756779",
      "      44216834 43816943 43706970 43837006 44497009",
      "      45306974 46356946 46976921"};

   CodedLocation location;
   bool          dataValid = location.Parse(data);

   EXPECT_EQ(dataValid, false);
}

TEST(CodedLocation, EmptyData)
{
   std::vector<std::string> data = {};

   CodedLocation location;
   bool          dataValid = location.Parse(data);

   EXPECT_EQ(dataValid, false);
}

TEST(CodedLocation, MalformedData)
{
   std::vector<std::string> data = {"LAT...LON 1360"};

   CodedLocation location;
   bool          dataValid = location.Parse(data);

   EXPECT_EQ(dataValid, false);
}

} // namespace awips
} // namespace scwx
