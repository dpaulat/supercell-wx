#include <scwx/wsr88d/rpg/scit_data_packet.hpp>
#include <scwx/wsr88d/rpg/packet_factory.hpp>
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

static const std::string logPrefix_ = "scwx::wsr88d::rpg::scit_data_packet";
static const auto        logger_    = util::Logger::Create(logPrefix_);

static const std::set<std::uint16_t> packetCodes_ = {23, 24};

class ScitDataPacket::Impl
{
public:
   explicit Impl() {}
   ~Impl() = default;

   std::vector<std::shared_ptr<Packet>> packetList_ {};
};

ScitDataPacket::ScitDataPacket() : p(std::make_unique<Impl>()) {}
ScitDataPacket::~ScitDataPacket() = default;

ScitDataPacket::ScitDataPacket(ScitDataPacket&&) noexcept            = default;
ScitDataPacket& ScitDataPacket::operator=(ScitDataPacket&&) noexcept = default;

std::vector<std::shared_ptr<Packet>> ScitDataPacket::packet_list() const
{
   return p->packetList_;
}

size_t ScitDataPacket::RecordCount() const
{
   return p->packetList_.size();
}

bool ScitDataPacket::ParseData(std::istream& is)
{
   bool blockValid = true;

   if (!packetCodes_.contains(packet_code()))
   {
      logger_->warn("Invalid packet code: {}", packet_code());
      blockValid = false;
   }

   if (blockValid)
   {
      std::uint32_t  bytesRead     = 0;
      std::uint32_t  lengthOfBlock = length_of_block();
      std::streampos dataStart     = is.tellg();
      std::streampos dataEnd =
         dataStart + static_cast<std::streamoff>(lengthOfBlock);

      while (bytesRead < lengthOfBlock)
      {
         std::shared_ptr<Packet> packet = PacketFactory::Create(is);
         if (packet != nullptr)
         {
            p->packetList_.push_back(packet);
            bytesRead += static_cast<std::uint32_t>(packet->data_size());
         }
         else
         {
            break;
         }
      }

      if (bytesRead < lengthOfBlock)
      {
         logger_->trace("Block bytes read smaller than size: {} < {} bytes",
                        bytesRead,
                        lengthOfBlock);
         blockValid = false;
         is.seekg(dataEnd, std::ios_base::beg);
      }
      if (bytesRead > lengthOfBlock)
      {
         logger_->warn("Block bytes read larger than size: {} > {} bytes",
                       bytesRead,
                       lengthOfBlock);
         blockValid = false;
         is.seekg(dataEnd, std::ios_base::beg);
      }
   }

   return blockValid;
}

std::shared_ptr<ScitDataPacket> ScitDataPacket::Create(std::istream& is)
{
   std::shared_ptr<ScitDataPacket> packet = std::make_shared<ScitDataPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
