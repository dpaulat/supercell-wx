#include <scwx/wsr88d/rpg/sti_circle_symbol_packet.hpp>

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
   "[scwx::wsr88d::rpg::sti_circle_symbol_packet] ";

struct StiCircleSymbol
{
   int16_t  iPosition_;
   int16_t  jPosition_;
   uint16_t radiusOfCircle_;

   StiCircleSymbol() : iPosition_ {0}, jPosition_ {0}, radiusOfCircle_ {0} {}
};

class StiCircleSymbolPacketImpl
{
public:
   explicit StiCircleSymbolPacketImpl() : symbol_ {}, recordCount_ {0} {}
   ~StiCircleSymbolPacketImpl() = default;

   std::vector<StiCircleSymbol> symbol_;
   size_t                       recordCount_;
};

StiCircleSymbolPacket::StiCircleSymbolPacket() :
    p(std::make_unique<StiCircleSymbolPacketImpl>())
{
}
StiCircleSymbolPacket::~StiCircleSymbolPacket() = default;

StiCircleSymbolPacket::StiCircleSymbolPacket(StiCircleSymbolPacket&&) noexcept =
   default;
StiCircleSymbolPacket&
StiCircleSymbolPacket::operator=(StiCircleSymbolPacket&&) noexcept = default;

int16_t StiCircleSymbolPacket::i_position(size_t i) const
{
   return p->symbol_[i].iPosition_;
}

int16_t StiCircleSymbolPacket::j_position(size_t i) const
{
   return p->symbol_[i].jPosition_;
}

uint16_t StiCircleSymbolPacket::radius_of_circle(size_t i) const
{
   return p->symbol_[i].radiusOfCircle_;
}

size_t StiCircleSymbolPacket::RecordCount() const
{
   return p->recordCount_;
}

bool StiCircleSymbolPacket::ParseData(std::istream& is)
{
   bool blockValid = true;

   if (packet_code() != 25)
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
         StiCircleSymbol& s = p->symbol_[i];

         is.read(reinterpret_cast<char*>(&s.iPosition_), 2);
         is.read(reinterpret_cast<char*>(&s.jPosition_), 2);
         is.read(reinterpret_cast<char*>(&s.radiusOfCircle_), 2);

         s.iPosition_      = ntohs(s.iPosition_);
         s.jPosition_      = ntohs(s.jPosition_);
         s.radiusOfCircle_ = ntohs(s.radiusOfCircle_);
      }
   }

   return blockValid;
}

std::shared_ptr<StiCircleSymbolPacket>
StiCircleSymbolPacket::Create(std::istream& is)
{
   std::shared_ptr<StiCircleSymbolPacket> packet =
      std::make_shared<StiCircleSymbolPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
