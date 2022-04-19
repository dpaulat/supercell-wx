#include <scwx/wsr88d/rpg/scit_forecast_data_packet.hpp>
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
   "scwx::wsr88d::rpg::scit_forecast_data_packet";
static const auto logger_ = util::Logger::Create(logPrefix_);

static const std::set<uint16_t> packetCodes_ = {23, 24};

class ScitForecastDataPacketImpl
{
public:
   explicit ScitForecastDataPacketImpl() : data_ {}, recordCount_ {0} {}
   ~ScitForecastDataPacketImpl() = default;

   std::vector<uint8_t> data_;
   size_t               recordCount_;
};

ScitForecastDataPacket::ScitForecastDataPacket() :
    p(std::make_unique<ScitForecastDataPacketImpl>())
{
}
ScitForecastDataPacket::~ScitForecastDataPacket() = default;

ScitForecastDataPacket::ScitForecastDataPacket(
   ScitForecastDataPacket&&) noexcept = default;
ScitForecastDataPacket&
ScitForecastDataPacket::operator=(ScitForecastDataPacket&&) noexcept = default;

const std::vector<uint8_t>& ScitForecastDataPacket::data() const
{
   return p->data_;
}

size_t ScitForecastDataPacket::RecordCount() const
{
   return p->recordCount_;
}

bool ScitForecastDataPacket::ParseData(std::istream& is)
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

std::shared_ptr<ScitForecastDataPacket>
ScitForecastDataPacket::Create(std::istream& is)
{
   std::shared_ptr<ScitForecastDataPacket> packet =
      std::make_shared<ScitForecastDataPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
