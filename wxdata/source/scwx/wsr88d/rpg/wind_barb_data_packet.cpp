#include <scwx/wsr88d/rpg/wind_barb_data_packet.hpp>
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
   "scwx::wsr88d::rpg::wind_barb_data_packet";
static const auto logger_ = util::Logger::Create(logPrefix_);

struct WindBarb
{
   uint16_t value_;
   int16_t  xCoordinate_;
   int16_t  yCoordinate_;
   uint16_t directionOfWind_;
   uint16_t windSpeed_;

   WindBarb() :
       value_ {0},
       xCoordinate_ {0},
       yCoordinate_ {0},
       directionOfWind_ {0},
       windSpeed_ {0}
   {
   }
};

class WindBarbDataPacketImpl
{
public:
   explicit WindBarbDataPacketImpl() :
       packetCode_ {0}, lengthOfBlock_ {0}, windBarb_ {}
   {
   }
   ~WindBarbDataPacketImpl() = default;

   uint16_t packetCode_;
   uint16_t lengthOfBlock_;

   std::vector<WindBarb> windBarb_;
};

WindBarbDataPacket::WindBarbDataPacket() :
    p(std::make_unique<WindBarbDataPacketImpl>())
{
}
WindBarbDataPacket::~WindBarbDataPacket() = default;

WindBarbDataPacket::WindBarbDataPacket(WindBarbDataPacket&&) noexcept = default;
WindBarbDataPacket&
WindBarbDataPacket::operator=(WindBarbDataPacket&&) noexcept = default;

uint16_t WindBarbDataPacket::packet_code() const
{
   return p->packetCode_;
}

uint16_t WindBarbDataPacket::length_of_block() const
{
   return p->lengthOfBlock_;
}

size_t WindBarbDataPacket::data_size() const
{
   return p->lengthOfBlock_ + 4u;
}

bool WindBarbDataPacket::Parse(std::istream& is)
{
   bool blockValid = true;

   std::streampos isBegin = is.tellg();

   is.read(reinterpret_cast<char*>(&p->packetCode_), 2);
   is.read(reinterpret_cast<char*>(&p->lengthOfBlock_), 2);

   p->packetCode_    = ntohs(p->packetCode_);
   p->lengthOfBlock_ = ntohs(p->lengthOfBlock_);

   if (is.eof())
   {
      logger_->debug("Reached end of file");
      blockValid = false;
   }
   else
   {
      if (p->packetCode_ != 4)
      {
         logger_->warn("Invalid packet code: {}", p->packetCode_);
         blockValid = false;
      }
   }

   // The number of vectors is equal to the size divided by the number of bytes
   // in a vector
   size_t vectorCount = p->lengthOfBlock_ / 10;

   p->windBarb_.resize(vectorCount);

   for (int v = 0; v < vectorCount && !is.eof(); v++)
   {
      WindBarb& windBarb = p->windBarb_[v];

      is.read(reinterpret_cast<char*>(&windBarb.value_), 2);
      is.read(reinterpret_cast<char*>(&windBarb.xCoordinate_), 2);
      is.read(reinterpret_cast<char*>(&windBarb.yCoordinate_), 2);
      is.read(reinterpret_cast<char*>(&windBarb.directionOfWind_), 2);
      is.read(reinterpret_cast<char*>(&windBarb.windSpeed_), 2);

      windBarb.value_           = ntohs(windBarb.value_);
      windBarb.xCoordinate_     = ntohs(windBarb.xCoordinate_);
      windBarb.yCoordinate_     = ntohs(windBarb.yCoordinate_);
      windBarb.directionOfWind_ = ntohs(windBarb.directionOfWind_);
      windBarb.windSpeed_       = ntohs(windBarb.windSpeed_);
   }

   std::streampos isEnd = is.tellg();

   if (!ValidateMessage(is, isEnd - isBegin))
   {
      blockValid = false;
   }

   return blockValid;
}

std::shared_ptr<WindBarbDataPacket> WindBarbDataPacket::Create(std::istream& is)
{
   std::shared_ptr<WindBarbDataPacket> packet =
      std::make_shared<WindBarbDataPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
