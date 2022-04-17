#include <scwx/wsr88d/rpg/point_graphic_symbol_packet.hpp>
#include <scwx/util/logger.hpp>

#include <istream>
#include <set>
#include <string>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ =
   "scwx::wsr88d::rpg::point_graphic_symbol_packet";
static const auto logger_ = util::Logger::Create(logPrefix_);

static const std::set<uint16_t> packetCodes_ = {12, 13, 14, 26};

struct PointGraphic
{
   int16_t iPosition_;
   int16_t jPosition_;

   PointGraphic() : iPosition_ {0}, jPosition_ {0} {}
};

class PointGraphicSymbolPacketImpl
{
public:
   explicit PointGraphicSymbolPacketImpl() : pointGraphic_ {}, recordCount_ {0}
   {
   }
   ~PointGraphicSymbolPacketImpl() = default;

   std::vector<PointGraphic> pointGraphic_;
   size_t                    recordCount_;
};

PointGraphicSymbolPacket::PointGraphicSymbolPacket() :
    p(std::make_unique<PointGraphicSymbolPacketImpl>())
{
}
PointGraphicSymbolPacket::~PointGraphicSymbolPacket() = default;

PointGraphicSymbolPacket::PointGraphicSymbolPacket(
   PointGraphicSymbolPacket&&) noexcept                     = default;
PointGraphicSymbolPacket& PointGraphicSymbolPacket::operator=(
   PointGraphicSymbolPacket&&) noexcept = default;

int16_t PointGraphicSymbolPacket::i_position(size_t i) const
{
   return p->pointGraphic_[i].iPosition_;
}

int16_t PointGraphicSymbolPacket::j_position(size_t i) const
{
   return p->pointGraphic_[i].jPosition_;
}

size_t PointGraphicSymbolPacket::RecordCount() const
{
   return p->recordCount_;
}

bool PointGraphicSymbolPacket::ParseData(std::istream& is)
{
   bool blockValid = true;

   if (!packetCodes_.contains(packet_code()))
   {
      logger_->warn("Invalid packet code: {}", packet_code());
      blockValid = false;
   }

   if (blockValid)
   {
      p->recordCount_ = length_of_block() / 4;
      p->pointGraphic_.resize(p->recordCount_);

      for (size_t i = 0; i < p->recordCount_; i++)
      {
         PointGraphic& f = p->pointGraphic_[i];

         is.read(reinterpret_cast<char*>(&f.iPosition_), 2);
         is.read(reinterpret_cast<char*>(&f.jPosition_), 2);

         f.iPosition_ = ntohs(f.iPosition_);
         f.jPosition_ = ntohs(f.jPosition_);
      }
   }

   return blockValid;
}

std::shared_ptr<PointGraphicSymbolPacket>
PointGraphicSymbolPacket::Create(std::istream& is)
{
   std::shared_ptr<PointGraphicSymbolPacket> packet =
      std::make_shared<PointGraphicSymbolPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
