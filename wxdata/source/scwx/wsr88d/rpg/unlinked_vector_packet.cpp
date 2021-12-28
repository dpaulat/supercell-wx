#include <scwx/wsr88d/rpg/unlinked_vector_packet.hpp>

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
   "[scwx::wsr88d::rpg::unlinked_vector_packet] ";

class UnlinkedVectorPacketImpl
{
public:
   explicit UnlinkedVectorPacketImpl() :
       packetCode_ {},
       lengthOfBlock_ {},
       valueOfVector_ {},
       beginI_ {},
       beginJ_ {},
       endI_ {},
       endJ_ {} {};
   ~UnlinkedVectorPacketImpl() = default;

   uint16_t packetCode_;
   uint16_t lengthOfBlock_;
   uint16_t valueOfVector_;

   std::vector<int16_t> beginI_;
   std::vector<int16_t> beginJ_;
   std::vector<int16_t> endI_;
   std::vector<int16_t> endJ_;
};

UnlinkedVectorPacket::UnlinkedVectorPacket() :
    p(std::make_unique<UnlinkedVectorPacketImpl>())
{
}
UnlinkedVectorPacket::~UnlinkedVectorPacket() = default;

UnlinkedVectorPacket::UnlinkedVectorPacket(UnlinkedVectorPacket&&) noexcept =
   default;
UnlinkedVectorPacket&
UnlinkedVectorPacket::operator=(UnlinkedVectorPacket&&) noexcept = default;

uint16_t UnlinkedVectorPacket::packet_code() const
{
   return p->packetCode_;
}

uint16_t UnlinkedVectorPacket::length_of_block() const
{
   return p->lengthOfBlock_;
}

std::optional<uint16_t> UnlinkedVectorPacket::value_of_vector() const
{
   std::optional<uint16_t> value;

   if (p->packetCode_ == 10)
   {
      value = p->valueOfVector_;
   }

   return value;
}

size_t UnlinkedVectorPacket::data_size() const
{
   return p->lengthOfBlock_ + 4u;
}

bool UnlinkedVectorPacket::Parse(std::istream& is)
{
   bool blockValid = true;

   is.read(reinterpret_cast<char*>(&p->packetCode_), 2);
   is.read(reinterpret_cast<char*>(&p->lengthOfBlock_), 2);

   p->packetCode_    = ntohs(p->packetCode_);
   p->lengthOfBlock_ = ntohs(p->lengthOfBlock_);

   int vectorSize = static_cast<int>(p->lengthOfBlock_);

   if (is.eof())
   {
      BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Reached end of file";
      blockValid = false;
   }
   else if (p->packetCode_ == 10)
   {
      is.read(reinterpret_cast<char*>(&p->valueOfVector_), 2);
      p->valueOfVector_ = ntohs(p->valueOfVector_);

      vectorSize -= 2;
   }

   // The number of vectors is equal to the size divided by the number of bytes
   // in a vector
   int     vectorCount = vectorSize / 8;
   int16_t beginI;
   int16_t beginJ;
   int16_t endI;
   int16_t endJ;

   for (int v = 0; v < vectorCount && !is.eof(); v++)
   {
      is.read(reinterpret_cast<char*>(&beginI), 2);
      is.read(reinterpret_cast<char*>(&beginJ), 2);
      is.read(reinterpret_cast<char*>(&endI), 2);
      is.read(reinterpret_cast<char*>(&endJ), 2);

      beginI = ntohs(beginI);
      beginJ = ntohs(beginJ);
      endI   = ntohs(endI);
      endJ   = ntohs(endJ);

      p->beginI_.push_back(beginI);
      p->beginJ_.push_back(beginJ);
      p->endI_.push_back(endI);
      p->endJ_.push_back(endJ);
   }

   if (is.eof())
   {
      BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Reached end of file";
      blockValid = false;
   }
   else
   {
      if (p->packetCode_ != 7 && p->packetCode_ != 10)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid packet code: " << p->packetCode_;
         blockValid = false;
      }
   }

   return blockValid;
}

std::shared_ptr<UnlinkedVectorPacket>
UnlinkedVectorPacket::Create(std::istream& is)
{
   std::shared_ptr<UnlinkedVectorPacket> packet =
      std::make_shared<UnlinkedVectorPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
