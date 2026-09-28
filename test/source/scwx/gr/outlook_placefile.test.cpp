#include <scwx/gr/outlook_placefile.hpp>
#include <scwx/gr/placefile.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>

#include <gtest/gtest.h>

namespace scwx
{
namespace gr
{

TEST(OutlookPlacefileTest, ConvertsRealDay1CategoricalOutlook)
{
   // Live-fetched from https://www.spc.noaa.gov/products/outlook/
   // day1otlk_cat.nolyr.geojson (2026-09-20) -- 3 risk-category features
   // (TSTM/MRGL/SLGT), each a MultiPolygon.
   std::string   filename(std::string(SCWX_TEST_DATA_DIR) +
                          "/gr/outlooks/day1_categorical.geojson");
   std::ifstream ifs {filename};
   ASSERT_TRUE(ifs.is_open());
   std::ostringstream buffer;
   buffer << ifs.rdbuf();

   const std::string placefileText = ConvertOutlookGeoJsonToPlacefile(
      buffer.str(), "SPC Day 1 Categorical Outlook", 600);

   ASSERT_FALSE(placefileText.empty());
   EXPECT_NE(placefileText.find("Title: SPC Day 1 Categorical Outlook"),
             std::string::npos);
   EXPECT_NE(placefileText.find("RefreshSeconds: 600"), std::string::npos);

   // Round-trip through the real placefile parser, same path
   // PlacefileManager itself uses for any other placefile.
   std::istringstream         placefileStream {placefileText};
   std::shared_ptr<Placefile> placefile =
      Placefile::Load("outlook-test", placefileStream);

   ASSERT_NE(placefile, nullptr);
   ASSERT_TRUE(placefile->IsValid());

   auto drawItems = placefile->GetDrawItems();

   // 3 risk categories in the fixture, each its own Polygon: block.
   std::size_t polygonCount = 0;
   for (auto& item : drawItems)
   {
      if (item->itemType_ == Placefile::ItemType::Polygon)
      {
         auto polygon =
            std::static_pointer_cast<Placefile::PolygonDrawItem>(item);

         // Each contour must be closed (first point == last point), per
         // the converter's own contract.
         for (auto& contour : polygon->contours_)
         {
            ASSERT_GE(contour.size(), 2u);
            EXPECT_DOUBLE_EQ(contour.front().latitude_,
                             contour.back().latitude_);
            EXPECT_DOUBLE_EQ(contour.front().longitude_,
                             contour.back().longitude_);
         }

         ++polygonCount;
      }
   }

   EXPECT_EQ(polygonCount, 3u);
}

TEST(OutlookPlacefileTest, ContourModeEmitsOneLinePerRing)
{
   // Same fixture as ConvertsRealDay1CategoricalOutlook: 3 features, 11
   // rings total across their (Multi)Polygon geometries (counted
   // independently from the raw GeoJSON, not derived from the converter
   // itself).
   std::string   filename(std::string(SCWX_TEST_DATA_DIR) +
                          "/gr/outlooks/day1_categorical.geojson");
   std::ifstream ifs {filename};
   ASSERT_TRUE(ifs.is_open());
   std::ostringstream buffer;
   buffer << ifs.rdbuf();

   const std::string placefileText =
      ConvertOutlookGeoJsonToPlacefile(buffer.str(),
                                       "SPC Day 1 Categorical Outlook",
                                       600,
                                       {},
                                       OutlookRenderMode::Contour,
                                       4);

   ASSERT_FALSE(placefileText.empty());
   EXPECT_EQ(placefileText.find("Polygon:"), std::string::npos);

   std::istringstream         placefileStream {placefileText};
   std::shared_ptr<Placefile> placefile =
      Placefile::Load("outlook-contour-test", placefileStream);

   ASSERT_NE(placefile, nullptr);
   ASSERT_TRUE(placefile->IsValid());

   auto drawItems = placefile->GetDrawItems();

   std::size_t lineCount = 0;
   for (auto& item : drawItems)
   {
      if (item->itemType_ != Placefile::ItemType::Line)
      {
         continue;
      }

      auto line = std::static_pointer_cast<Placefile::LineDrawItem>(item);
      EXPECT_DOUBLE_EQ(line->width_, 4.0);
      ASSERT_GE(line->elements_.size(), 2u);

      // Each ring's own closure (first point repeating as last, see
      // WriteRing) makes the polyline's own start/end connect back up --
      // same closed-loop check as the Fill-mode tests, just against
      // LineDrawItem::elements_ instead of PolygonDrawItem::contours_.
      EXPECT_DOUBLE_EQ(line->elements_.front().latitude_,
                       line->elements_.back().latitude_);
      EXPECT_DOUBLE_EQ(line->elements_.front().longitude_,
                       line->elements_.back().longitude_);

      ++lineCount;
   }

   EXPECT_EQ(lineCount, 11u);
}

TEST(OutlookPlacefileTest, ConvertsRealWpcExcessiveRainfallOutlook)
{
   // Live-fetched from https://www.wpc.ncep.noaa.gov/exper/eromap/geojson/
   // Day1_Latest.geojson (2026-09-20) -- 4 features (2x Marginal, 2x
   // Slight), each a plain Polygon (not MultiPolygon, unlike the SPC
   // fixture -- incidentally exercises that code path too). Unlike SPC,
   // WPC's GeoJSON has no fill/stroke, only "dn" -- this is the
   // kWpcEroColorTable fallback path.
   std::string   filename(std::string(SCWX_TEST_DATA_DIR) +
                          "/gr/outlooks/day1_ero.geojson");
   std::ifstream ifs {filename};
   ASSERT_TRUE(ifs.is_open());
   std::ostringstream buffer;
   buffer << ifs.rdbuf();

   const std::string placefileText =
      ConvertOutlookGeoJsonToPlacefile(buffer.str(),
                                       "WPC Day 1 Excessive Rainfall Outlook",
                                       1800,
                                       kWpcEroColorTable);

   ASSERT_FALSE(placefileText.empty());

   std::istringstream         placefileStream {placefileText};
   std::shared_ptr<Placefile> placefile =
      Placefile::Load("ero-test", placefileStream);

   ASSERT_NE(placefile, nullptr);
   ASSERT_TRUE(placefile->IsValid());

   auto drawItems = placefile->GetDrawItems();

   std::size_t marginalCount = 0;
   std::size_t slightCount   = 0;
   for (auto& item : drawItems)
   {
      if (item->itemType_ != Placefile::ItemType::Polygon)
      {
         continue;
      }

      auto polygon = std::static_pointer_cast<Placefile::PolygonDrawItem>(item);
      ASSERT_FALSE(polygon->contours_.empty());
      for (auto& contour : polygon->contours_)
      {
         ASSERT_GE(contour.size(), 2u);
         EXPECT_DOUBLE_EQ(contour.front().latitude_, contour.back().latitude_);
         EXPECT_DOUBLE_EQ(contour.front().longitude_,
                          contour.back().longitude_);
      }

      if (polygon->color_[0] == 0 && polygon->color_[1] == 255 &&
          polygon->color_[2] == 0)
      {
         ++marginalCount;
      }
      else if (polygon->color_[0] == 255 && polygon->color_[1] == 255 &&
               polygon->color_[2] == 0)
      {
         ++slightCount;
      }
   }

   EXPECT_EQ(marginalCount, 2u);
   EXPECT_EQ(slightCount, 2u);
}

TEST(OutlookPlacefileTest, EmptyOrInvalidGeoJsonYieldsEmptyPlacefile)
{
   EXPECT_TRUE(
      ConvertOutlookGeoJsonToPlacefile("not json", "Bad Input", 600).empty());
   EXPECT_TRUE(
      ConvertOutlookGeoJsonToPlacefile("{}", "No Features", 600).empty());
}

namespace
{

// KMZ is binary (a zip archive), so this reads raw bytes rather than the
// text-mode ifstream the GeoJSON tests above use.
std::string LoadDay1QpfKmz()
{
   std::string   filename(std::string(SCWX_TEST_DATA_DIR) +
                          "/gr/outlooks/day1_qpf.kmz");
   std::ifstream ifs {filename, std::ios::binary};
   if (!ifs.is_open())
   {
      return {};
   }
   std::ostringstream buffer;
   buffer << ifs.rdbuf();
   return buffer.str();
}

} // namespace

TEST(OutlookPlacefileTest, ConvertsRealDay1QpfKmz)
{
   // Trimmed from a live-fetched https://www.wpc.ncep.noaa.gov/kml/qpf/
   // QPF24hr_Day1_latest.kmz (2026-09-26) down to 3 of its real 18
   // Placemarks (kept verbatim, not synthesized): "QPF 3.00 inches" (5
   // outerBoundaryIs + 1 innerBoundaryIs = 6 rings), "QPF 4.00 inches" (1
   // ring), and "QPF 5.00 inches" (a real empty <MultiGeometry> -- no
   // areas reached that amount that run -- and no <styleUrl> at all,
   // exercising the "skip: no color, no rings" path from both sides at
   // once). Unlike SPC/WPC ERO's GeoJSON, color comes from a *shared*
   // named <Style> each Placemark references via <styleUrl>, in KML's
   // own "aabbggrr" byte order -- Style3_00 is "ff0000cd" (opaque,
   // RGB 205/0/0), Style4_00 is "ff0040ee" (opaque, RGB 238/64/0).
   const std::string kmzBytes = LoadDay1QpfKmz();
   ASSERT_FALSE(kmzBytes.empty());

   const std::string placefileText =
      ConvertOutlookKmzToPlacefile(kmzBytes, "WPC Day 1 QPF", 1200);

   ASSERT_FALSE(placefileText.empty());
   EXPECT_NE(placefileText.find("Title: WPC Day 1 QPF"), std::string::npos);
   EXPECT_NE(placefileText.find("RefreshSeconds: 1200"), std::string::npos);
   // Contour mode is this function's own default (see its header doc).
   EXPECT_EQ(placefileText.find("Polygon:"), std::string::npos);

   std::istringstream         placefileStream {placefileText};
   std::shared_ptr<Placefile> placefile =
      Placefile::Load("qpf-kmz-test", placefileStream);

   ASSERT_NE(placefile, nullptr);
   ASSERT_TRUE(placefile->IsValid());

   auto drawItems = placefile->GetDrawItems();

   std::size_t lineCount   = 0;
   std::size_t redCount    = 0; // Style3_00, RGB 205/0/0
   std::size_t orangeCount = 0; // Style4_00, RGB 238/64/0
   for (auto& item : drawItems)
   {
      if (item->itemType_ != Placefile::ItemType::Line)
      {
         continue;
      }

      auto line = std::static_pointer_cast<Placefile::LineDrawItem>(item);
      ASSERT_GE(line->elements_.size(), 2u);

      // Same closed-loop check as the GeoJSON contour test -- confirms
      // ParseKmlCoordinates() preserved KML's own already-closed rings
      // without double-closing them.
      EXPECT_DOUBLE_EQ(line->elements_.front().latitude_,
                       line->elements_.back().latitude_);
      EXPECT_DOUBLE_EQ(line->elements_.front().longitude_,
                       line->elements_.back().longitude_);

      if (line->color_[0] == 205 && line->color_[1] == 0 &&
          line->color_[2] == 0)
      {
         ++redCount;
      }
      else if (line->color_[0] == 238 && line->color_[1] == 64 &&
               line->color_[2] == 0)
      {
         ++orangeCount;
      }

      ++lineCount;
   }

   EXPECT_EQ(lineCount, 7u); // 6 rings (3.00in) + 1 ring (4.00in)
   EXPECT_EQ(redCount, 6u);
   EXPECT_EQ(orangeCount, 1u);
}

TEST(OutlookPlacefileTest, FillModeKmzGroupsRingsIntoOnePolygonPerFeature)
{
   const std::string kmzBytes = LoadDay1QpfKmz();
   ASSERT_FALSE(kmzBytes.empty());

   const std::string placefileText = ConvertOutlookKmzToPlacefile(
      kmzBytes, "WPC Day 1 QPF", 1200, OutlookRenderMode::Fill);

   ASSERT_FALSE(placefileText.empty());
   EXPECT_EQ(placefileText.find("Line:"), std::string::npos);

   std::istringstream         placefileStream {placefileText};
   std::shared_ptr<Placefile> placefile =
      Placefile::Load("qpf-kmz-fill-test", placefileStream);

   ASSERT_NE(placefile, nullptr);
   ASSERT_TRUE(placefile->IsValid());

   auto drawItems = placefile->GetDrawItems();

   // 2 real, non-empty features ("5.00 inches" has no color and no
   // rings, so it contributes nothing) -- one Polygon: block each,
   // holding its own feature's rings as separate contours (6 for
   // "3.00 inches", 1 for "4.00 inches").
   std::vector<std::size_t> contourCounts;
   for (auto& item : drawItems)
   {
      if (item->itemType_ == Placefile::ItemType::Polygon)
      {
         auto polygon =
            std::static_pointer_cast<Placefile::PolygonDrawItem>(item);
         contourCounts.push_back(polygon->contours_.size());
      }
   }

   ASSERT_EQ(contourCounts.size(), 2u);
   std::sort(contourCounts.begin(), contourCounts.end());
   EXPECT_EQ(contourCounts, (std::vector<std::size_t> {1u, 6u}));
}

} // namespace gr
} // namespace scwx
