#include <scwx/wsr88d/rpg/special_graphic_symbol_packet.hpp>

#include <istream>
#include <set>
#include <string>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ =
   "[scwx::wsr88d::rpg::special_graphic_symbol_packet] ";

static const std::set<uint16_t> packetCodes_ = {
   3, 11, 12, 13, 14, 15, 19, 20, 23, 24, 25, 26};

class SpecialGraphicSymbolPacketImpl
{
public:
   explicit SpecialGraphicSymbolPacketImpl() :
       packetCode_ {0}, lengthOfBlock_ {0}
   {
   }
   ~SpecialGraphicSymbolPacketImpl() = default;

   uint16_t packetCode_;
   uint16_t lengthOfBlock_;
};

SpecialGraphicSymbolPacket::SpecialGraphicSymbolPacket() :
    p(std::make_unique<SpecialGraphicSymbolPacketImpl>())
{
}
SpecialGraphicSymbolPacket::~SpecialGraphicSymbolPacket() = default;

SpecialGraphicSymbolPacket::SpecialGraphicSymbolPacket(
   SpecialGraphicSymbolPacket&&) noexcept                       = default;
SpecialGraphicSymbolPacket& SpecialGraphicSymbolPacket::operator=(
   SpecialGraphicSymbolPacket&&) noexcept = default;

uint16_t SpecialGraphicSymbolPacket::packet_code() const
{
   return p->packetCode_;
}

uint16_t SpecialGraphicSymbolPacket::length_of_block() const
{
   return p->lengthOfBlock_;
}

size_t SpecialGraphicSymbolPacket::data_size() const
{
   return p->lengthOfBlock_ + 4u;
}

bool SpecialGraphicSymbolPacket::Parse(std::istream& is)
{
   bool blockValid = true;

   std::streampos isBegin = is.tellg();

   is.read(reinterpret_cast<char*>(&p->packetCode_), 2);
   is.read(reinterpret_cast<char*>(&p->lengthOfBlock_), 2);

   p->packetCode_    = ntohs(p->packetCode_);
   p->lengthOfBlock_ = ntohs(p->lengthOfBlock_);

   if (is.eof())
   {
      BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Reached end of file";
      blockValid = false;
   }
   else
   {
      const size_t minBlockLength = MinBlockLength();
      const size_t maxBlockLength = MaxBlockLength();

      if (!packetCodes_.contains(p->packetCode_))
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid packet code: " << p->packetCode_;
         blockValid = false;
      }
      else if (p->lengthOfBlock_ < minBlockLength ||
               p->lengthOfBlock_ > maxBlockLength)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid length of block: " << p->packetCode_;
         blockValid = false;
      }
   }

   if (blockValid)
   {
      blockValid = ParseData(is);
   }

   std::streampos isEnd = is.tellg();

   if (!ValidateMessage(is, isEnd - isBegin))
   {
      blockValid = false;
   }

   return blockValid;
}

size_t SpecialGraphicSymbolPacket::MinBlockLength() const
{
   return 1;
}

size_t SpecialGraphicSymbolPacket::MaxBlockLength() const
{
   return 32767;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
