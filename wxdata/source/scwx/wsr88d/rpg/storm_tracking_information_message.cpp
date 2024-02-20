#include <scwx/wsr88d/rpg/storm_tracking_information_message.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ =
   "scwx::wsr88d::rpg::storm_tracking_information_message";
static const auto logger_ = util::Logger::Create(logPrefix_);

class StormTrackingInformationMessage::Impl
{
public:
   explicit Impl() {}
   ~Impl() = default;
};

StormTrackingInformationMessage::StormTrackingInformationMessage() :
    p(std::make_unique<Impl>())
{
}
StormTrackingInformationMessage::~StormTrackingInformationMessage() = default;

StormTrackingInformationMessage::StormTrackingInformationMessage(
   StormTrackingInformationMessage&&) noexcept = default;
StormTrackingInformationMessage& StormTrackingInformationMessage::operator=(
   StormTrackingInformationMessage&&) noexcept = default;

bool StormTrackingInformationMessage::Parse(std::istream& is)
{
   bool dataValid = GraphicProductMessage::Parse(is);

   return dataValid;
}

std::shared_ptr<StormTrackingInformationMessage>
StormTrackingInformationMessage::Create(Level3MessageHeader&& header,
                                        std::istream&         is)
{
   std::shared_ptr<StormTrackingInformationMessage> message =
      std::make_shared<StormTrackingInformationMessage>();
   message->set_header(std::move(header));

   if (!message->Parse(is))
   {
      message.reset();
   }

   return message;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
