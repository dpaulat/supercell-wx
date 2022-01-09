#include <scwx/wsr88d/rpg/generic_data_packet.hpp>

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
   "[scwx::wsr88d::rpg::generic_data_packet] ";

class GenericDataPacketImpl
{
public:
   explicit GenericDataPacketImpl() :
       packetCode_ {0}, lengthOfBlock_ {0}, data_ {}
   {
   }
   ~GenericDataPacketImpl() = default;

   uint16_t packetCode_;
   uint32_t lengthOfBlock_;

   std::vector<uint8_t> data_;
};

GenericDataPacket::GenericDataPacket() :
    p(std::make_unique<GenericDataPacketImpl>())
{
}
GenericDataPacket::~GenericDataPacket() = default;

GenericDataPacket::GenericDataPacket(GenericDataPacket&&) noexcept = default;
GenericDataPacket&
GenericDataPacket::operator=(GenericDataPacket&&) noexcept = default;

uint16_t GenericDataPacket::packet_code() const
{
   return p->packetCode_;
}

uint32_t GenericDataPacket::length_of_block() const
{
   return p->lengthOfBlock_;
}

size_t GenericDataPacket::data_size() const
{
   return p->lengthOfBlock_ + 8u;
}

bool GenericDataPacket::Parse(std::istream& is)
{
   bool blockValid = true;

   std::streampos isBegin = is.tellg();

   is.read(reinterpret_cast<char*>(&p->packetCode_), 2);
   is.seekg(2, std::ios_base::cur);
   is.read(reinterpret_cast<char*>(&p->lengthOfBlock_), 4);

   p->packetCode_    = ntohs(p->packetCode_);
   p->lengthOfBlock_ = ntohl(p->lengthOfBlock_);

   if (is.eof())
   {
      BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Reached end of file";
      blockValid = false;
   }
   else
   {
      if (p->packetCode_ != 28 && p->packetCode_ != 29)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid packet code: " << p->packetCode_;
         blockValid = false;
      }
   }

   p->data_.resize(p->lengthOfBlock_);
   is.read(reinterpret_cast<char*>(p->data_.data()), p->lengthOfBlock_);

   std::streampos isEnd = is.tellg();

   if (!ValidateMessage(is, isEnd - isBegin))
   {
      blockValid = false;
   }

   return blockValid;
}

std::shared_ptr<GenericDataPacket> GenericDataPacket::Create(std::istream& is)
{
   std::shared_ptr<GenericDataPacket> packet =
      std::make_shared<GenericDataPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
