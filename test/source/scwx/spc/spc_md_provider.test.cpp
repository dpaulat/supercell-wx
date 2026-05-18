#include <scwx/spc/spc_md_provider.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace spc
{

static const std::string logPrefix_ = "scwx::spc::spc_md_provider.test";

TEST(SpcMdProviderTest, ParseKmlWithOnePlacemark)
{
   std::string kml = R"(
<?xml version="1.0" encoding="UTF-8"?>
<kml xmlns="http://www.opengis.net/kml/2.2">
  <Document>
    <Placemark>
      <name>MD #1234</name>
      <description><![CDATA[<p>Test discussion text.</p>]]></description>
      <Polygon>
        <outerBoundaryIs>
          <LinearRing>
            <coordinates>
              -98.0,35.0,0 -97.0,35.0,0 -97.0,36.0,0 -98.0,36.0,0 -98.0,35.0,0
            </coordinates>
          </LinearRing>
        </outerBoundaryIs>
      </Polygon>
    </Placemark>
  </Document>
</kml>
)";

   MdData data = SpcMdProvider::ParseKml(kml);

   ASSERT_EQ(data.discussions_.size(), 1u);
   EXPECT_EQ(data.discussions_[0].mdNumber_, 1234);
   EXPECT_EQ(data.discussions_[0].name_, "MD #1234");
   EXPECT_EQ(data.discussions_[0].description_, "<p>Test discussion text.</p>");

   // Verify coordinates (should be lat,lon)
   ASSERT_EQ(data.discussions_[0].rings_.size(), 1u);
   const auto& ring = data.discussions_[0].rings_.front();
   ASSERT_EQ(ring.size(), 5u);
   EXPECT_DOUBLE_EQ(ring[0].first, 35.0);   // lat
   EXPECT_DOUBLE_EQ(ring[0].second, -98.0); // lon
   EXPECT_DOUBLE_EQ(ring[2].first, 36.0);   // lat
   EXPECT_DOUBLE_EQ(ring[2].second, -97.0); // lon

   // Verify centroid
   EXPECT_DOUBLE_EQ(data.discussions_[0].centroid_.latitude_, 35.4);
   EXPECT_DOUBLE_EQ(data.discussions_[0].centroid_.longitude_, -97.5);
}

TEST(SpcMdProviderTest, ParseKmlEmpty)
{
   std::string kml = R"(
<?xml version="1.0" encoding="UTF-8"?>
<kml xmlns="http://www.opengis.net/kml/2.2">
  <Document>
  </Document>
</kml>
)";

   MdData data = SpcMdProvider::ParseKml(kml);
   ASSERT_TRUE(data.discussions_.empty());
}

TEST(SpcMdProviderTest, ParseKmlInvalidXml)
{
   std::string kml  = "not valid xml";
   MdData      data = SpcMdProvider::ParseKml(kml);
   ASSERT_TRUE(data.discussions_.empty());
}

TEST(SpcMdProviderTest, ParseKmlNoName)
{
   std::string kml = R"(
<?xml version="1.0" encoding="UTF-8"?>
<kml xmlns="http://www.opengis.net/kml/2.2">
  <Document>
    <Placemark>
      <description>No number here</description>
    </Placemark>
  </Document>
</kml>
)";

   MdData data = SpcMdProvider::ParseKml(kml);
   ASSERT_TRUE(data.discussions_.empty());
}

} // namespace spc
} // namespace scwx
