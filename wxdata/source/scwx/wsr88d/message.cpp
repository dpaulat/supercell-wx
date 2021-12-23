#include <scwx/wsr88d/message.hpp>

namespace scwx
{
namespace wsr88d
{

static const std::string logPrefix_ = "[scwx::wsr88d::message] ";

class MessageImpl
{
public:
   explicit MessageImpl() {};
   ~MessageImpl() = default;
};

Message::Message() : p(std::make_unique<MessageImpl>()) {}
Message::~Message() = default;

Message::Message(Message&&) noexcept = default;
Message& Message::operator=(Message&&) noexcept = default;

} // namespace wsr88d
} // namespace scwx
