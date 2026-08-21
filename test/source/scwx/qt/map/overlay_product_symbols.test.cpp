#include <scwx/qt/map/overlay_product_symbols.hpp>

#include <gtest/gtest.h>

namespace scwx::qt::map
{

TEST(OverlayProductSymbols, OverlayProductNames)
{
   EXPECT_TRUE(IsOverlayProduct("NST"));
   EXPECT_TRUE(IsOverlayProduct("NHI"));
   EXPECT_TRUE(IsOverlayProduct("NMD"));
   EXPECT_TRUE(IsOverlayProduct("NTV"));
   EXPECT_TRUE(IsOverlayProduct("NME"));
   EXPECT_FALSE(IsOverlayProduct("N0B"));

   EXPECT_TRUE(IsOverlayProductCode(58));
   EXPECT_TRUE(IsOverlayProductCode(59));
   EXPECT_TRUE(IsOverlayProductCode(60));
   EXPECT_TRUE(IsOverlayProductCode(61));
   EXPECT_TRUE(IsOverlayProductCode(141));
   EXPECT_FALSE(IsOverlayProductCode(94));
}

TEST(OverlayProductSymbols, HailIconIndex)
{
   EXPECT_FALSE(HailSymbolVisible(-999));
   EXPECT_FALSE(HailSymbolVisible(-1));
   EXPECT_TRUE(HailSymbolVisible(0));
   EXPECT_TRUE(HailSymbolVisible(80));

   EXPECT_EQ(HailIconIndex(80, 0, 0), kHailIconSmall);
   EXPECT_EQ(HailIconIndex(100, 0, 1), kHailIconMedium);
   EXPECT_EQ(HailIconIndex(100, 0, 2), kHailIconLarge);
   EXPECT_EQ(HailIconIndex(100, 40, 1), kHailIconLarge);
   EXPECT_EQ(HailIconIndex(100, 50, 2), kHailIconSevere);
}

TEST(OverlayProductSymbols, HailHoverText)
{
   EXPECT_EQ(FormatHailSize(0), "<0.25 in");
   EXPECT_EQ(FormatHailSize(1), "1 in");
   EXPECT_EQ(FormatProbability(-999), "N/A");
   EXPECT_EQ(FormatProbability(80), "80%");

   const std::string hover = HailHoverText("P3", 100, 40, 1);
   EXPECT_NE(hover.find("Storm ID: P3"), std::string::npos);
   EXPECT_NE(hover.find("100%"), std::string::npos);
   EXPECT_NE(hover.find("40%"), std::string::npos);
   EXPECT_NE(hover.find("1 in"), std::string::npos);
}

TEST(OverlayProductSymbols, MesocycloneIcons)
{
   EXPECT_TRUE(IsMesocycloneFeatureType(9));
   EXPECT_TRUE(IsMesocycloneFeatureType(11));
   EXPECT_FALSE(IsMesocycloneFeatureType(7));

   EXPECT_EQ(MesocycloneIconIndexFromFeatureType(9), kMesoIconLowLevelStrong);
   EXPECT_EQ(MesocycloneIconIndexFromFeatureType(10), kMesoIconElevatedStrong);
   EXPECT_EQ(MesocycloneIconIndexFromFeatureType(3), kMesoIconCirculation);
   EXPECT_EQ(MesocycloneIconIndexFromFeatureType(11), kMesoIconShear);
   EXPECT_EQ(MesocycloneIconIndexFromPacketCode(3), kMesoIconCirculation);
   EXPECT_EQ(MesocycloneIconIndexFromPacketCode(11), kMesoIconShear);

   const std::string hover = MesocycloneHoverText("65", 9, 5);
   EXPECT_NE(hover.find("Circulation ID: 65"), std::string::npos);
   EXPECT_NE(hover.find("1.25 km"), std::string::npos);
}

TEST(OverlayProductSymbols, TvsIcons)
{
   EXPECT_TRUE(IsTvsFeatureType(5));
   EXPECT_TRUE(IsTvsFeatureType(8));
   EXPECT_FALSE(IsTvsFeatureType(9));

   EXPECT_EQ(TvsIconIndexFromFeatureType(7), kTvsIconTvs);
   EXPECT_EQ(TvsIconIndexFromFeatureType(8), kTvsIconEtvs);
   EXPECT_EQ(TvsIconIndexFromPacketCode(12), kTvsIconTvs);
   EXPECT_EQ(TvsIconIndexFromPacketCode(26), kTvsIconEtvs);

   const std::string hover = TvsHoverText("M9", "TVS");
   EXPECT_NE(hover.find("Storm ID: M9"), std::string::npos);
   EXPECT_NE(hover.find("Type: TVS"), std::string::npos);
}

TEST(OverlayProductSymbols, TrimLabel)
{
   EXPECT_EQ(TrimLabel("  659 "), "659");
   EXPECT_EQ(TrimLabel(std::string("65") + '\0' + "xx"), "65");
}

} // namespace scwx::qt::map
