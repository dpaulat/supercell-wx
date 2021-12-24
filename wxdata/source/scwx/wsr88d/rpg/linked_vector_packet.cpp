#include <scwx/wsr88d/rpg/linked_vector_packet.hpp>
#include <scwx/wsr88d/rpg/vector2d.hpp>

#include <array>
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
   "[scwx::wsr88d::rpg::linked_vector_packet] ";

class LinkedVectorPacketImpl
{
public:
   explicit LinkedVectorPacketImpl() :
       packetCode_ {}, lengthOfBlock_ {}, valueOfVector_ {}, vectorList_ {} {};
   ~LinkedVectorPacketImpl() = default;

   uint16_t packetCode_;
   uint16_t lengthOfBlock_;
   uint16_t valueOfVector_;

   std::vector<Vector2D> vectorList_;
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

uint16_t LinkedVectorPacket::value_of_vector() const
{
   return p->valueOfVector_;
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

   size_t vectorSize = p->lengthOfBlock_;

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

   // The number of vectors is equal to the size divided by the number of bytes
   // in a vector
   size_t vectorCount = Vector2D::SIZE / 8;

   for (size_t v = 0; v < vectorCount && !is.eof(); v++)
   {
      Vector2D vector;
      vector.Parse(is);
      p->vectorList_.push_back(std::move(vector));
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

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
