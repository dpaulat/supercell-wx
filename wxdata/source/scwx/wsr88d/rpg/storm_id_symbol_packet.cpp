#include <scwx/wsr88d/rpg/storm_id_symbol_packet.hpp>

#include <istream>
#include <string>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ =
   "[scwx::wsr88d::rpg::storm_id_symbol_packet] ";

struct StormIdSymbol
{
   int16_t             iPosition_;
   int16_t             jPosition_;
   std::array<char, 2> character_;

   StormIdSymbol() : iPosition_ {0}, jPosition_ {0}, character_ {0} {}
};

class StormIdSymbolPacketImpl
{
public:
   explicit StormIdSymbolPacketImpl() : symbol_ {}, recordCount_ {0} {}
   ~StormIdSymbolPacketImpl() = default;

   std::vector<StormIdSymbol> symbol_;
   size_t                     recordCount_;
};

StormIdSymbolPacket::StormIdSymbolPacket() :
    p(std::make_unique<StormIdSymbolPacketImpl>())
{
}
StormIdSymbolPacket::~StormIdSymbolPacket() = default;

StormIdSymbolPacket::StormIdSymbolPacket(StormIdSymbolPacket&&) noexcept =
   default;
StormIdSymbolPacket&
StormIdSymbolPacket::operator=(StormIdSymbolPacket&&) noexcept = default;

int16_t StormIdSymbolPacket::i_position(size_t i) const
{
   return p->symbol_[i].iPosition_;
}

int16_t StormIdSymbolPacket::j_position(size_t i) const
{
   return p->symbol_[i].jPosition_;
}

const std::array<char, 2>& StormIdSymbolPacket::character(size_t i) const
{
   return p->symbol_[i].character_;
}

size_t StormIdSymbolPacket::RecordCount() const
{
   return p->recordCount_;
}

bool StormIdSymbolPacket::ParseData(std::istream& is)
{
   bool blockValid = true;

   if (packet_code() != 15)
   {
      BOOST_LOG_TRIVIAL(warning)
         << logPrefix_ << "Invalid packet code: " << packet_code();
      blockValid = false;
   }

   if (blockValid)
   {
      p->recordCount_ = length_of_block() / 6;
      p->symbol_.resize(p->recordCount_);

      for (size_t i = 0; i < p->recordCount_; i++)
      {
         StormIdSymbol& s = p->symbol_[i];

         is.read(reinterpret_cast<char*>(&s.iPosition_), 2);
         is.read(reinterpret_cast<char*>(&s.jPosition_), 2);
         is.read(reinterpret_cast<char*>(s.character_.data()), 2);

         s.iPosition_ = ntohs(s.iPosition_);
         s.jPosition_ = ntohs(s.jPosition_);
      }
   }

   return blockValid;
}

std::shared_ptr<StormIdSymbolPacket>
StormIdSymbolPacket::Create(std::istream& is)
{
   std::shared_ptr<StormIdSymbolPacket> packet =
      std::make_shared<StormIdSymbolPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
