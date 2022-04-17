#include <scwx/wsr88d/rpg/unlinked_vector_packet.hpp>
#include <scwx/util/logger.hpp>

#include <istream>
#include <string>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ =
   "scwx::wsr88d::rpg::unlinked_vector_packet";
static const auto logger_ = util::Logger::Create(logPrefix_);

class UnlinkedVectorPacketImpl
{
public:
   explicit UnlinkedVectorPacketImpl() :
       packetCode_ {0},
       lengthOfBlock_ {0},
       valueOfVector_ {0},
       beginI_ {},
       beginJ_ {},
       endI_ {},
       endJ_ {}
   {
   }
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

   std::streampos isBegin = is.tellg();

   is.read(reinterpret_cast<char*>(&p->packetCode_), 2);
   is.read(reinterpret_cast<char*>(&p->lengthOfBlock_), 2);

   p->packetCode_    = ntohs(p->packetCode_);
   p->lengthOfBlock_ = ntohs(p->lengthOfBlock_);

   int vectorSize = static_cast<int>(p->lengthOfBlock_);

   if (is.eof())
   {
      logger_->debug("Reached end of file");
      blockValid = false;
   }
   else if (p->packetCode_ == 10)
   {
      is.read(reinterpret_cast<char*>(&p->valueOfVector_), 2);
      p->valueOfVector_ = ntohs(p->valueOfVector_);

      vectorSize -= 2;
   }
   else
   {
      if (p->packetCode_ != 7 && p->packetCode_ != 10)
      {
         logger_->warn("Invalid packet code: {}", p->packetCode_);
         blockValid = false;
      }
   }

   if (blockValid)
   {
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
