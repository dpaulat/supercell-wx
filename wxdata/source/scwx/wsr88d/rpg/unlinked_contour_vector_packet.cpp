#include <scwx/wsr88d/rpg/unlinked_contour_vector_packet.hpp>

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
   "[scwx::wsr88d::rpg::unlinked_contour_vector_packet] ";

class UnlinkedContourVectorPacketImpl
{
public:
   explicit UnlinkedContourVectorPacketImpl() :
       packetCode_ {0},
       lengthOfVectors_ {0},
       beginI_ {},
       beginJ_ {},
       endI_ {},
       endJ_ {}
   {
   }
   ~UnlinkedContourVectorPacketImpl() = default;

   uint16_t packetCode_;
   uint16_t lengthOfVectors_;
   uint16_t valueOfVector_;

   std::vector<int16_t> beginI_;
   std::vector<int16_t> beginJ_;
   std::vector<int16_t> endI_;
   std::vector<int16_t> endJ_;
};

UnlinkedContourVectorPacket::UnlinkedContourVectorPacket() :
    p(std::make_unique<UnlinkedContourVectorPacketImpl>())
{
}
UnlinkedContourVectorPacket::~UnlinkedContourVectorPacket() = default;

UnlinkedContourVectorPacket::UnlinkedContourVectorPacket(
   UnlinkedContourVectorPacket&&) noexcept                        = default;
UnlinkedContourVectorPacket& UnlinkedContourVectorPacket::operator=(
   UnlinkedContourVectorPacket&&) noexcept = default;

uint16_t UnlinkedContourVectorPacket::packet_code() const
{
   return p->packetCode_;
}

uint16_t UnlinkedContourVectorPacket::length_of_vectors() const
{
   return p->lengthOfVectors_ + 4u;
   ;
}

size_t UnlinkedContourVectorPacket::data_size() const
{
   return p->lengthOfVectors_ + 4u;
}

bool UnlinkedContourVectorPacket::Parse(std::istream& is)
{
   bool blockValid = true;

   std::streampos isBegin = is.tellg();

   is.read(reinterpret_cast<char*>(&p->packetCode_), 2);
   is.read(reinterpret_cast<char*>(&p->lengthOfVectors_), 2);

   p->packetCode_      = ntohs(p->packetCode_);
   p->lengthOfVectors_ = ntohs(p->lengthOfVectors_);

   if (is.eof())
   {
      BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Reached end of file";
      blockValid = false;
   }
   else
   {
      if (p->packetCode_ != 0x3501)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid packet code: " << p->packetCode_;
         blockValid = false;
      }
   }

   if (blockValid)
   {
      int vectorSize = static_cast<int>(p->lengthOfVectors_);

      // The number of vectors is equal to the size divided by the number of
      // bytes in a vector
      int vectorCount = vectorSize / 8;

      p->beginI_.resize(vectorCount);
      p->beginJ_.resize(vectorCount);
      p->endI_.resize(vectorCount);
      p->endJ_.resize(vectorCount);

      for (int v = 0; v < vectorCount && !is.eof(); v++)
      {
         is.read(reinterpret_cast<char*>(&p->beginI_[v]), 2);
         is.read(reinterpret_cast<char*>(&p->beginJ_[v]), 2);
         is.read(reinterpret_cast<char*>(&p->endI_[v]), 2);
         is.read(reinterpret_cast<char*>(&p->endJ_[v]), 2);

         p->beginI_[v] = ntohs(p->beginI_[v]);
         p->beginJ_[v] = ntohs(p->beginJ_[v]);
         p->endI_[v]   = ntohs(p->endI_[v]);
         p->endJ_[v]   = ntohs(p->endJ_[v]);
      }
   }

   std::streampos isEnd     = is.tellg();
   std::streamoff bytesRead = isEnd - isBegin;

   if (!ValidateMessage(is, bytesRead))
   {
      blockValid = false;
   }

   return blockValid;
}

std::shared_ptr<UnlinkedContourVectorPacket>
UnlinkedContourVectorPacket::Create(std::istream& is)
{
   std::shared_ptr<UnlinkedContourVectorPacket> packet =
      std::make_shared<UnlinkedContourVectorPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
