#include <scwx/wsr88d/rpg/mesocyclone_symbol_packet.hpp>
#include <scwx/util/logger.hpp>

#include <istream>
#include <set>
#include <string>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ =
   "scwx::wsr88d::rpg::mesocyclone_symbol_packet";
static const auto logger_ = util::Logger::Create(logPrefix_);

static const std::set<uint16_t> packetCodes_ = {3, 11};

struct MesocycloneSymbol
{
   int16_t iPosition_;
   int16_t jPosition_;
   int16_t radiusOfMesocyclone_;

   MesocycloneSymbol() :
       iPosition_ {0}, jPosition_ {0}, radiusOfMesocyclone_ {0}
   {
   }
};

class MesocycloneSymbolPacketImpl
{
public:
   explicit MesocycloneSymbolPacketImpl() : symbol_ {}, recordCount_ {0} {}
   ~MesocycloneSymbolPacketImpl() = default;

   std::vector<MesocycloneSymbol> symbol_;
   size_t                         recordCount_;
};

MesocycloneSymbolPacket::MesocycloneSymbolPacket() :
    p(std::make_unique<MesocycloneSymbolPacketImpl>())
{
}
MesocycloneSymbolPacket::~MesocycloneSymbolPacket() = default;

MesocycloneSymbolPacket::MesocycloneSymbolPacket(
   MesocycloneSymbolPacket&&) noexcept                    = default;
MesocycloneSymbolPacket& MesocycloneSymbolPacket::operator=(
   MesocycloneSymbolPacket&&) noexcept = default;

int16_t MesocycloneSymbolPacket::i_position(size_t i) const
{
   return p->symbol_[i].iPosition_;
}

int16_t MesocycloneSymbolPacket::j_position(size_t i) const
{
   return p->symbol_[i].jPosition_;
}

int16_t MesocycloneSymbolPacket::radius_of_mesocyclone(size_t i) const
{
   return p->symbol_[i].radiusOfMesocyclone_;
}

size_t MesocycloneSymbolPacket::RecordCount() const
{
   return p->recordCount_;
}

bool MesocycloneSymbolPacket::ParseData(std::istream& is)
{
   bool blockValid = true;

   if (!packetCodes_.contains(packet_code()))
   {
      logger_->warn("Invalid packet code: {}", packet_code());
      blockValid = false;
   }

   if (blockValid)
   {
      p->recordCount_ = length_of_block() / 6;
      p->symbol_.resize(p->recordCount_);

      for (size_t i = 0; i < p->recordCount_; i++)
      {
         MesocycloneSymbol& s = p->symbol_[i];

         is.read(reinterpret_cast<char*>(&s.iPosition_), 2);
         is.read(reinterpret_cast<char*>(&s.jPosition_), 2);
         is.read(reinterpret_cast<char*>(&s.radiusOfMesocyclone_), 2);

         s.iPosition_           = ntohs(s.iPosition_);
         s.jPosition_           = ntohs(s.jPosition_);
         s.radiusOfMesocyclone_ = ntohs(s.radiusOfMesocyclone_);
      }
   }

   return blockValid;
}

std::shared_ptr<MesocycloneSymbolPacket>
MesocycloneSymbolPacket::Create(std::istream& is)
{
   std::shared_ptr<MesocycloneSymbolPacket> packet =
      std::make_shared<MesocycloneSymbolPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
