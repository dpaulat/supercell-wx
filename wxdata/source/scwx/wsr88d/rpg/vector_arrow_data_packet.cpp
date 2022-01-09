#include <scwx/wsr88d/rpg/vector_arrow_data_packet.hpp>

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
   "[scwx::wsr88d::rpg::vector_arrow_data_packet] ";

struct VectorArrow
{
   int16_t  iCoordinatePoint_;
   int16_t  jCoordinatePoint_;
   uint16_t directionOfArrow_;
   uint16_t arrowLength_;
   uint16_t arrowHeadLength_;

   VectorArrow() :
       iCoordinatePoint_ {0},
       jCoordinatePoint_ {0},
       directionOfArrow_ {0},
       arrowLength_ {0},
       arrowHeadLength_ {0}
   {
   }
};

class VectorArrowDataPacketImpl
{
public:
   explicit VectorArrowDataPacketImpl() :
       packetCode_ {0}, lengthOfBlock_ {0}, arrow_ {}
   {
   }
   ~VectorArrowDataPacketImpl() = default;

   uint16_t packetCode_;
   uint16_t lengthOfBlock_;

   std::vector<VectorArrow> arrow_;
};

VectorArrowDataPacket::VectorArrowDataPacket() :
    p(std::make_unique<VectorArrowDataPacketImpl>())
{
}
VectorArrowDataPacket::~VectorArrowDataPacket() = default;

VectorArrowDataPacket::VectorArrowDataPacket(VectorArrowDataPacket&&) noexcept =
   default;
VectorArrowDataPacket&
VectorArrowDataPacket::operator=(VectorArrowDataPacket&&) noexcept = default;

uint16_t VectorArrowDataPacket::packet_code() const
{
   return p->packetCode_;
}

uint16_t VectorArrowDataPacket::length_of_block() const
{
   return p->lengthOfBlock_;
}

size_t VectorArrowDataPacket::data_size() const
{
   return p->lengthOfBlock_ + 4u;
}

bool VectorArrowDataPacket::Parse(std::istream& is)
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
      if (p->packetCode_ != 5)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid packet code: " << p->packetCode_;
         blockValid = false;
      }
   }

   // The number of vectors is equal to the size divided by the number of bytes
   // in a vector
   size_t vectorCount = p->lengthOfBlock_ / 10;

   p->arrow_.resize(vectorCount);

   for (int v = 0; v < vectorCount && !is.eof(); v++)
   {
      VectorArrow& arrow = p->arrow_[v];

      is.read(reinterpret_cast<char*>(&arrow.iCoordinatePoint_), 2);
      is.read(reinterpret_cast<char*>(&arrow.jCoordinatePoint_), 2);
      is.read(reinterpret_cast<char*>(&arrow.directionOfArrow_), 2);
      is.read(reinterpret_cast<char*>(&arrow.arrowLength_), 2);
      is.read(reinterpret_cast<char*>(&arrow.arrowHeadLength_), 2);

      arrow.iCoordinatePoint_ = ntohs(arrow.iCoordinatePoint_);
      arrow.jCoordinatePoint_ = ntohs(arrow.jCoordinatePoint_);
      arrow.directionOfArrow_ = ntohs(arrow.directionOfArrow_);
      arrow.arrowLength_      = ntohs(arrow.arrowLength_);
      arrow.arrowHeadLength_  = ntohs(arrow.arrowHeadLength_);
   }

   std::streampos isEnd = is.tellg();

   if (!ValidateMessage(is, isEnd - isBegin))
   {
      blockValid = false;
   }

   return blockValid;
}

std::shared_ptr<VectorArrowDataPacket>
VectorArrowDataPacket::Create(std::istream& is)
{
   std::shared_ptr<VectorArrowDataPacket> packet =
      std::make_shared<VectorArrowDataPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
