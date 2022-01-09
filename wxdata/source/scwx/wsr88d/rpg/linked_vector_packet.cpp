#include <scwx/wsr88d/rpg/linked_vector_packet.hpp>

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
   "[scwx::wsr88d::rpg::linked_vector_packet] ";

class LinkedVectorPacketImpl
{
public:
   explicit LinkedVectorPacketImpl() :
       packetCode_ {0},
       lengthOfBlock_ {0},
       valueOfVector_ {0},
       startI_ {0},
       startJ_ {0},
       endI_ {},
       endJ_ {}
   {
   }
   ~LinkedVectorPacketImpl() = default;

   uint16_t packetCode_;
   uint16_t lengthOfBlock_;
   uint16_t valueOfVector_;

   int16_t              startI_;
   int16_t              startJ_;
   std::vector<int16_t> endI_;
   std::vector<int16_t> endJ_;
};

LinkedVectorPacket::LinkedVectorPacket() :
    p(std::make_unique<LinkedVectorPacketImpl>())
{
}
LinkedVectorPacket::~LinkedVectorPacket() = default;

LinkedVectorPacket::LinkedVectorPacket(LinkedVectorPacket&&) noexcept = default;
LinkedVectorPacket&
LinkedVectorPacket::operator=(LinkedVectorPacket&&) noexcept = default;

uint16_t LinkedVectorPacket::packet_code() const
{
   return p->packetCode_;
}

uint16_t LinkedVectorPacket::length_of_block() const
{
   return p->lengthOfBlock_;
}

std::optional<uint16_t> LinkedVectorPacket::value_of_vector() const
{
   std::optional<uint16_t> value;

   if (p->packetCode_ == 9)
   {
      value = p->valueOfVector_;
   }

   return value;
}

size_t LinkedVectorPacket::data_size() const
{
   return p->lengthOfBlock_ + 4u;
}

bool LinkedVectorPacket::Parse(std::istream& is)
{
   bool blockValid = true;

   is.read(reinterpret_cast<char*>(&p->packetCode_), 2);
   is.read(reinterpret_cast<char*>(&p->lengthOfBlock_), 2);

   p->packetCode_    = ntohs(p->packetCode_);
   p->lengthOfBlock_ = ntohs(p->lengthOfBlock_);

   int vectorSize = static_cast<int>(p->lengthOfBlock_) - 4;

   if (is.eof())
   {
      BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Reached end of file";
      blockValid = false;
   }
   else if (p->packetCode_ == 9)
   {
      is.read(reinterpret_cast<char*>(&p->valueOfVector_), 2);
      p->valueOfVector_ = ntohs(p->valueOfVector_);

      vectorSize -= 2;
   }

   is.read(reinterpret_cast<char*>(&p->startI_), 2);
   is.read(reinterpret_cast<char*>(&p->startJ_), 2);

   p->startI_ = ntohs(p->startI_);
   p->startJ_ = ntohs(p->startJ_);

   // The number of vectors is equal to the size divided by the number of bytes
   // in a vector coordinate
   int vectorCount = vectorSize / 4;

   p->endI_.resize(vectorCount);
   p->endJ_.resize(vectorCount);

   for (int v = 0; v < vectorCount && !is.eof(); v++)
   {
      is.read(reinterpret_cast<char*>(&p->endI_[v]), 2);
      is.read(reinterpret_cast<char*>(&p->endJ_[v]), 2);

      p->endI_[v] = ntohs(p->endI_[v]);
      p->endJ_[v] = ntohs(p->endJ_[v]);
   }

   if (is.eof())
   {
      BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Reached end of file";
      blockValid = false;
   }
   else
   {
      if (p->packetCode_ != 6 && p->packetCode_ != 9)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid packet code: " << p->packetCode_;
         blockValid = false;
      }
   }

   return blockValid;
}

std::shared_ptr<LinkedVectorPacket> LinkedVectorPacket::Create(std::istream& is)
{
   std::shared_ptr<LinkedVectorPacket> packet =
      std::make_shared<LinkedVectorPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
