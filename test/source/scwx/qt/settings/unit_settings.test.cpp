#include <scwx/qt/settings/unit_settings.hpp>
#include <scwx/qt/types/unit_types.hpp>

#include <boost/algorithm/string.hpp>
#include <gtest/gtest.h>

namespace scwx::qt::settings
{

TEST(UnitSettingsTest, RadarBeamHeightReferenceDefaultsToArl)
{
   const UnitSettings settings;

   EXPECT_EQ(settings.radar_beam_height_reference().GetValue(),
             "above radar level");
   EXPECT_EQ(types::GetRadarBeamHeightReferenceFromName(
                settings.radar_beam_height_reference().GetValue()),
             types::RadarBeamHeightReference::AboveRadarLevel);
}

TEST(UnitSettingsTest, RadarBeamHeightReferenceParticipatesInEquality)
{
   const UnitSettings lhs;
   const UnitSettings rhs;

   EXPECT_TRUE(lhs == rhs);

   std::string amslName = types::GetRadarBeamHeightReferenceName(
      types::RadarBeamHeightReference::AboveMeanSeaLevel);
   boost::to_lower(amslName);

   EXPECT_TRUE(lhs.radar_beam_height_reference().SetValue(amslName));
   EXPECT_FALSE(lhs == rhs);
}

TEST(UnitSettingsTest, RadarBeamHeightReferenceAbbreviations)
{
   EXPECT_EQ(types::GetRadarBeamHeightReferenceAbbreviation(
                types::RadarBeamHeightReference::AboveRadarLevel),
             "ARL");
   EXPECT_EQ(types::GetRadarBeamHeightReferenceAbbreviation(
                types::RadarBeamHeightReference::AboveMeanSeaLevel),
             "AMSL");
}

} // namespace scwx::qt::settings
