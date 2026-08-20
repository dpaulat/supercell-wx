#include <scwx/qt/settings/unit_settings.hpp>
#include <scwx/qt/types/unit_types.hpp>

#include <boost/algorithm/string.hpp>
#include <gtest/gtest.h>

namespace scwx::qt::settings
{

TEST(UnitSettingsTest, RadarBeamHeightReferenceDefaultsToMsl)
{
   const UnitSettings settings;

   EXPECT_EQ(settings.radar_beam_height_reference().GetValue(),
             "mean sea level");
   EXPECT_EQ(types::GetRadarBeamHeightReferenceFromName(
                settings.radar_beam_height_reference().GetValue()),
             types::RadarBeamHeightReference::MeanSeaLevel);
}

TEST(UnitSettingsTest, RadarBeamHeightReferenceParticipatesInEquality)
{
   const UnitSettings lhs;
   const UnitSettings rhs;

   EXPECT_TRUE(lhs == rhs);

   std::string arlName = types::GetRadarBeamHeightReferenceName(
      types::RadarBeamHeightReference::AboveRadarLevel);
   boost::to_lower(arlName);

   EXPECT_TRUE(lhs.radar_beam_height_reference().SetValue(arlName));
   EXPECT_FALSE(lhs == rhs);
}

TEST(UnitSettingsTest, RadarBeamHeightReferenceAbbreviations)
{
   EXPECT_EQ(types::GetRadarBeamHeightReferenceAbbreviation(
                types::RadarBeamHeightReference::AboveRadarLevel),
             "ARL");
   EXPECT_EQ(types::GetRadarBeamHeightReferenceAbbreviation(
                types::RadarBeamHeightReference::MeanSeaLevel),
             "MSL");
}

} // namespace scwx::qt::settings
