#include <scwx/wsr88d/rpg/packet_factory.hpp>

#include <scwx/wsr88d/rpg/cell_trend_data_packet.hpp>
#include <scwx/wsr88d/rpg/digital_precipitation_data_array_packet.hpp>
#include <scwx/wsr88d/rpg/digital_radial_data_array_packet.hpp>
#include <scwx/wsr88d/rpg/generic_data_packet.hpp>
#include <scwx/wsr88d/rpg/hda_hail_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/linked_contour_vector_packet.hpp>
#include <scwx/wsr88d/rpg/linked_vector_packet.hpp>
#include <scwx/wsr88d/rpg/mesocyclone_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/point_feature_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/point_graphic_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/precipitation_rate_data_array_packet.hpp>
#include <scwx/wsr88d/rpg/radial_data_packet.hpp>
#include <scwx/wsr88d/rpg/raster_data_packet.hpp>
#include <scwx/wsr88d/rpg/scit_forecast_data_packet.hpp>
#include <scwx/wsr88d/rpg/set_color_level_packet.hpp>
#include <scwx/wsr88d/rpg/sti_circle_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/storm_id_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/text_and_special_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/unlinked_contour_vector_packet.hpp>
#include <scwx/wsr88d/rpg/unlinked_vector_packet.hpp>
#include <scwx/wsr88d/rpg/vector_arrow_data_packet.hpp>
#include <scwx/wsr88d/rpg/wind_barb_data_packet.hpp>

#include <unordered_map>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ = "[scwx::wsr88d::rpg::packet_factory] ";

typedef std::function<std::shared_ptr<Packet>(std::istream&)>
   CreateMessageFunction;

static const std::unordered_map<uint16_t, CreateMessageFunction> create_ {
   {1, TextAndSpecialSymbolPacket::Create},
   {2, TextAndSpecialSymbolPacket::Create},
   {3, MesocycloneSymbolPacket::Create},
   {4, WindBarbDataPacket::Create},
   {5, VectorArrowDataPacket::Create},
   {6, LinkedVectorPacket::Create},
   {7, UnlinkedVectorPacket::Create},
   {8, TextAndSpecialSymbolPacket::Create},
   {9, LinkedVectorPacket::Create},
   {10, UnlinkedVectorPacket::Create},
   {11, MesocycloneSymbolPacket::Create},
   {12, PointGraphicSymbolPacket::Create},
   {13, PointGraphicSymbolPacket::Create},
   {14, PointGraphicSymbolPacket::Create},
   {15, StormIdSymbolPacket::Create},
   {16, DigitalRadialDataArrayPacket::Create},
   {17, DigitalPrecipitationDataArrayPacket::Create},
   {18, PrecipitationRateDataArrayPacket::Create},
   {19, HdaHailSymbolPacket::Create},
   {20, PointFeatureSymbolPacket::Create},
   {21, CellTrendDataPacket::Create},
   {23, ScitForecastDataPacket::Create},
   {24, ScitForecastDataPacket::Create},
   {25, StiCircleSymbolPacket::Create},
   {26, PointGraphicSymbolPacket::Create},
   {28, GenericDataPacket::Create},
   {29, GenericDataPacket::Create},
   {0x0802, SetColorLevelPacket::Create},
   {0x0E03, LinkedContourVectorPacket::Create},
   {0x3501, UnlinkedContourVectorPacket::Create},
   {0xAF1F, RadialDataPacket::Create},
   {0xBA07, RasterDataPacket::Create},
   {0xBA0F, RasterDataPacket::Create}};

std::shared_ptr<Packet> PacketFactory::Create(std::istream& is)
{
   std::shared_ptr<Packet> packet      = nullptr;
   bool                    packetValid = true;

   uint16_t packetCode;

   is.read(reinterpret_cast<char*>(&packetCode), 2);
   packetCode = ntohs(packetCode);

   if (is.eof())
   {
      packetValid = false;
   }

   is.seekg(-2, std::ios_base::cur);

   if (packetValid && create_.find(packetCode) == create_.end())
   {
      BOOST_LOG_TRIVIAL(warning)
         << logPrefix_ << "Unknown packet code: " << packetCode << " (0x"
         << std::hex << packetCode << std::dec << ")";
      packetValid = false;
   }

   if (packetValid)
   {
      BOOST_LOG_TRIVIAL(trace)
         << logPrefix_ << "Found packet code: " << packetCode << " (0x"
         << std::hex << packetCode << std::dec << ")";
      packet = create_.at(packetCode)(is);
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
