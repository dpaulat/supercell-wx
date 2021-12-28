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
       packetCode_ {},
       lengthOfVectors_ {},
       beginI_ {},
       beginJ_ {},
       endI_ {},
       endJ_ {} {};
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

   is.read(reinterpret_cast<char*>(&p->packetCode_), 2);
   is.read(reinterpret_cast<char*>(&p->lengthOfVectors_), 2);

   p->packetCode_      = ntohs(p->packetCode_);
   p->lengthOfVectors_ = ntohs(p->lengthOfVectors_);

   int vectorSize = static_cast<int>(p->lengthOfVectors_);

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
      if (p->packetCode_ != 0x3501)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid packet code: " << p->packetCode_;
         blockValid = false;
      }
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
