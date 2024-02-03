#include <scwx/wsr88d/rpg/scit_data_packet.hpp>
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
   explicit Impl() : data_ {}, recordCount_ {0} {}
   ~Impl() = default;

   std::vector<std::uint8_t> data_;
   size_t                    recordCount_;
};

ScitDataPacket::ScitDataPacket() : p(std::make_unique<Impl>()) {}
ScitDataPacket::~ScitDataPacket() = default;

ScitDataPacket::ScitDataPacket(ScitDataPacket&&) noexcept            = default;
ScitDataPacket& ScitDataPacket::operator=(ScitDataPacket&&) noexcept = default;

const std::vector<std::uint8_t>& ScitDataPacket::data() const
{
   return p->data_;
}

size_t ScitDataPacket::RecordCount() const
{
   return p->recordCount_;
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
      p->recordCount_ = length_of_block();
      p->data_.resize(p->recordCount_);
      is.read(reinterpret_cast<char*>(p->data_.data()), p->recordCount_);
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
