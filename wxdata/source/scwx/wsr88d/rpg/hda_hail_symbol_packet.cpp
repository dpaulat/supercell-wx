#include <scwx/wsr88d/rpg/hda_hail_symbol_packet.hpp>

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
   "[scwx::wsr88d::rpg::hda_hail_symbol_packet] ";

struct HdaHailSymbol
{
   int16_t  iPosition_;
   int16_t  jPosition_;
   int16_t  probabilityOfHail_;
   int16_t  probabilityOfSevereHail_;
   uint16_t maxHailSize_;

   HdaHailSymbol() :
       iPosition_ {0},
       jPosition_ {0},
       probabilityOfHail_ {0},
       probabilityOfSevereHail_ {0},
       maxHailSize_ {0}
   {
   }
};

class HdaHailSymbolPacketImpl
{
public:
   explicit HdaHailSymbolPacketImpl() : symbol_ {}, recordCount_ {0} {}
   ~HdaHailSymbolPacketImpl() = default;

   std::vector<HdaHailSymbol> symbol_;
   size_t                     recordCount_;
};

HdaHailSymbolPacket::HdaHailSymbolPacket() :
    p(std::make_unique<HdaHailSymbolPacketImpl>())
{
}
HdaHailSymbolPacket::~HdaHailSymbolPacket() = default;

HdaHailSymbolPacket::HdaHailSymbolPacket(HdaHailSymbolPacket&&) noexcept =
   default;
HdaHailSymbolPacket&
HdaHailSymbolPacket::operator=(HdaHailSymbolPacket&&) noexcept = default;

int16_t HdaHailSymbolPacket::i_position(size_t i) const
{
   return p->symbol_[i].iPosition_;
}

int16_t HdaHailSymbolPacket::j_position(size_t i) const
{
   return p->symbol_[i].jPosition_;
}

int16_t HdaHailSymbolPacket::probability_of_hail(size_t i) const
{
   return p->symbol_[i].probabilityOfHail_;
}

int16_t HdaHailSymbolPacket::probability_of_severe_hail(size_t i) const
{
   return p->symbol_[i].probabilityOfSevereHail_;
}

uint16_t HdaHailSymbolPacket::max_hail_size(size_t i) const
{
   return p->symbol_[i].maxHailSize_;
}

size_t HdaHailSymbolPacket::RecordCount() const
{
   return p->recordCount_;
}

bool HdaHailSymbolPacket::ParseData(std::istream& is)
{
   bool blockValid = true;

   if (packet_code() != 19)
   {
      BOOST_LOG_TRIVIAL(warning)
         << logPrefix_ << "Invalid packet code: " << packet_code();
      blockValid = false;
   }

   if (blockValid)
   {
      p->recordCount_ = length_of_block() / 10;
      p->symbol_.resize(p->recordCount_);

      for (size_t i = 0; i < p->recordCount_; i++)
      {
         HdaHailSymbol& s = p->symbol_[i];

         is.read(reinterpret_cast<char*>(&s.iPosition_), 2);
         is.read(reinterpret_cast<char*>(&s.jPosition_), 2);
         is.read(reinterpret_cast<char*>(&s.probabilityOfHail_), 2);
         is.read(reinterpret_cast<char*>(&s.probabilityOfSevereHail_), 2);
         is.read(reinterpret_cast<char*>(&s.maxHailSize_), 2);

         s.iPosition_               = ntohs(s.iPosition_);
         s.jPosition_               = ntohs(s.jPosition_);
         s.probabilityOfHail_       = ntohs(s.probabilityOfHail_);
         s.probabilityOfSevereHail_ = ntohs(s.probabilityOfSevereHail_);
         s.maxHailSize_             = ntohs(s.maxHailSize_);
      }
   }

   return blockValid;
}

std::shared_ptr<HdaHailSymbolPacket>
HdaHailSymbolPacket::Create(std::istream& is)
{
   std::shared_ptr<HdaHailSymbolPacket> packet =
      std::make_shared<HdaHailSymbolPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
