#include <scwx/wsr88d/rpg/set_color_level_packet.hpp>

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
   "[scwx::wsr88d::rpg::set_color_level_packet] ";

class SetColorLevelPacketImpl
{
public:
   explicit SetColorLevelPacketImpl() :
       packetCode_ {}, colorValueIndicator_ {}, valueOfContour_ {} {};
   ~SetColorLevelPacketImpl() = default;

   uint16_t packetCode_;
   uint16_t colorValueIndicator_;
   uint16_t valueOfContour_;
};

SetColorLevelPacket::SetColorLevelPacket() :
    p(std::make_unique<SetColorLevelPacketImpl>())
{
}
SetColorLevelPacket::~SetColorLevelPacket() = default;

SetColorLevelPacket::SetColorLevelPacket(SetColorLevelPacket&&) noexcept =
   default;
SetColorLevelPacket&
SetColorLevelPacket::operator=(SetColorLevelPacket&&) noexcept = default;

uint16_t SetColorLevelPacket::packet_code() const
{
   return p->packetCode_;
}

uint16_t SetColorLevelPacket::color_value_indicator() const
{
   return p->colorValueIndicator_;
}

uint16_t SetColorLevelPacket::value_of_contour() const
{
   return p->valueOfContour_;
}

size_t SetColorLevelPacket::data_size() const
{
   return SIZE;
}

bool SetColorLevelPacket::Parse(std::istream& is)
{
   bool blockValid = true;

   is.read(reinterpret_cast<char*>(&p->packetCode_), 2);
   is.read(reinterpret_cast<char*>(&p->colorValueIndicator_), 2);
   is.read(reinterpret_cast<char*>(&p->valueOfContour_), 2);

   p->packetCode_          = ntohs(p->packetCode_);
   p->colorValueIndicator_ = ntohs(p->colorValueIndicator_);
   p->valueOfContour_      = ntohs(p->valueOfContour_);

   if (is.eof())
   {
      BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Reached end of file";
      blockValid = false;
   }
   else
   {
      if (p->packetCode_ != 0x0802)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid packet code: " << p->packetCode_;
         blockValid = false;
      }
      if (p->colorValueIndicator_ != 0x0002)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_
            << "Invalid color value indicator: " << p->colorValueIndicator_;
         blockValid = false;
      }
   }

   return blockValid;
}

std::shared_ptr<SetColorLevelPacket>
SetColorLevelPacket::Create(std::istream& is)
{
   std::shared_ptr<SetColorLevelPacket> packet =
      std::make_shared<SetColorLevelPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
