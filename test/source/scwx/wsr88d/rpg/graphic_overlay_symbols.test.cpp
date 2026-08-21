#include <scwx/wsr88d/level3_file.hpp>
#include <scwx/wsr88d/rpg/graphic_product_message.hpp>
#include <scwx/wsr88d/rpg/hda_hail_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/point_feature_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/point_graphic_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/storm_id_symbol_packet.hpp>

#include <gtest/gtest.h>

namespace scwx::wsr88d
{

namespace
{

std::shared_ptr<rpg::GraphicProductMessage>
LoadGraphicProduct(const std::string& filename)
{
   Level3File file;
   const bool fileValid = file.LoadFile(std::string(SCWX_TEST_DATA_DIR) +
                                        "/nexrad/level3/" + filename);
   EXPECT_TRUE(fileValid);
   auto message = file.message();
   EXPECT_NE(message, nullptr);
   return std::dynamic_pointer_cast<rpg::GraphicProductMessage>(message);
}

} // namespace

TEST(GraphicOverlaySymbols, HailIndex)
{
   auto gpm = LoadGraphicProduct("KLSX_SDUS63_NHILSX_202112110152");
   ASSERT_NE(gpm, nullptr);
   EXPECT_EQ(gpm->header().message_code(), 59);

   auto psb = gpm->symbology_block();
   ASSERT_NE(psb, nullptr);
   ASSERT_GE(psb->number_of_layers(), 1);

   auto        packets      = psb->packet_list(0);
   std::size_t hailCount    = 0;
   std::size_t stormIdCount = 0;
   bool        foundP3      = false;

   for (const auto& packet : packets)
   {
      if (auto hail =
             std::dynamic_pointer_cast<rpg::HdaHailSymbolPacket>(packet))
      {
         hailCount += hail->RecordCount();
         for (std::size_t i = 0; i < hail->RecordCount(); ++i)
         {
            if (hail->i_position(i) == -687 && hail->j_position(i) == -395)
            {
               EXPECT_EQ(hail->probability_of_hail(i), 100);
               EXPECT_EQ(hail->probability_of_severe_hail(i), 40);
               EXPECT_EQ(hail->max_hail_size(i), 1);
            }
         }
      }
      else if (auto stormId =
                  std::dynamic_pointer_cast<rpg::StormIdSymbolPacket>(packet))
      {
         stormIdCount += stormId->RecordCount();
         for (std::size_t i = 0; i < stormId->RecordCount(); ++i)
         {
            if (stormId->storm_id(i) == "P3")
            {
               foundP3 = true;
               EXPECT_EQ(stormId->i_position(i), -687);
               EXPECT_EQ(stormId->j_position(i), -395);
            }
         }
      }
   }

   EXPECT_GT(hailCount, 0);
   EXPECT_GT(stormIdCount, 0);
   EXPECT_TRUE(foundP3);
}

TEST(GraphicOverlaySymbols, MesocycloneDetection)
{
   auto gpm = LoadGraphicProduct("KLSX_SDUS33_NMDLSX_202112110152");
   ASSERT_NE(gpm, nullptr);
   EXPECT_EQ(gpm->header().message_code(), 141);

   auto psb = gpm->symbology_block();
   ASSERT_NE(psb, nullptr);
   ASSERT_GE(psb->number_of_layers(), 1);

   auto        packets       = psb->packet_list(0);
   std::size_t featureCount  = 0;
   bool        foundLowLevel = false;

   for (const auto& packet : packets)
   {
      auto feature =
         std::dynamic_pointer_cast<rpg::PointFeatureSymbolPacket>(packet);
      if (feature == nullptr)
      {
         continue;
      }

      featureCount += feature->RecordCount();
      for (std::size_t i = 0; i < feature->RecordCount(); ++i)
      {
         if (feature->point_feature_type(i) == 9)
         {
            foundLowLevel = true;
            EXPECT_GT(feature->point_feature_attribute(i), 0);
         }
      }
   }

   EXPECT_GT(featureCount, 0);
   EXPECT_TRUE(foundLowLevel);
}

TEST(GraphicOverlaySymbols, TornadicVortexSignature)
{
   auto gpm = LoadGraphicProduct("KLSX_SDUS63_NTVLSX_202112110238");
   ASSERT_NE(gpm, nullptr);
   EXPECT_EQ(gpm->header().message_code(), 61);

   auto psb = gpm->symbology_block();
   ASSERT_NE(psb, nullptr);
   ASSERT_GE(psb->number_of_layers(), 1);

   auto        packets  = psb->packet_list(0);
   std::size_t tvsCount = 0;
   bool        foundM9  = false;

   for (const auto& packet : packets)
   {
      if (auto tvs =
             std::dynamic_pointer_cast<rpg::PointGraphicSymbolPacket>(packet))
      {
         tvsCount += tvs->RecordCount();
         EXPECT_EQ(tvs->packet_code(), 12);
         EXPECT_EQ(tvs->i_position(0), 284);
         EXPECT_EQ(tvs->j_position(0), 70);
      }
      else if (auto stormId =
                  std::dynamic_pointer_cast<rpg::StormIdSymbolPacket>(packet))
      {
         if (stormId->RecordCount() > 0 && stormId->storm_id(0) == "M9")
         {
            foundM9 = true;
         }
      }
   }

   EXPECT_EQ(tvsCount, 1);
   EXPECT_TRUE(foundM9);
}

} // namespace scwx::wsr88d
