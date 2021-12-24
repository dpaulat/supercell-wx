#include <scwx/wsr88d/rda/level2_message.hpp>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

static const std::string logPrefix_ = "[scwx::wsr88d::rda::level2_message] ";

class Level2MessageImpl
{
public:
   explicit Level2MessageImpl() : header_() {};
   ~Level2MessageImpl() = default;

   Level2MessageHeader header_;
};

Level2Message::Level2Message() :
    Message(), p(std::make_unique<Level2MessageImpl>())
{
}
Level2Message::~Level2Message() = default;

Level2Message::Level2Message(Level2Message&&) noexcept = default;
Level2Message& Level2Message::operator=(Level2Message&&) noexcept = default;

size_t Level2Message::data_size() const
{
   return (header().message_size() * 2 - header().SIZE);
}

const Level2MessageHeader& Level2Message::header() const
{
   return p->header_;
}

void Level2Message::set_header(Level2MessageHeader&& header)
{
   p->header_ = std::move(header);
}

} // namespace rda
} // namespace wsr88d
} // namespace scwx
