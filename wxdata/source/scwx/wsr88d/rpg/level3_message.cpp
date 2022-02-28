#include <scwx/wsr88d/rpg/level3_message.hpp>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ = "[scwx::wsr88d::rpg::level3_message] ";

class Level3MessageImpl
{
public:
   explicit Level3MessageImpl() : header_() {};
   ~Level3MessageImpl() = default;

   Level3MessageHeader header_;
};

Level3Message::Level3Message() :
    Message(), p(std::make_unique<Level3MessageImpl>())
{
}
Level3Message::~Level3Message() = default;

Level3Message::Level3Message(Level3Message&&) noexcept = default;
Level3Message& Level3Message::operator=(Level3Message&&) noexcept = default;

size_t Level3Message::data_size() const
{
   return (header().length_of_message() - header().SIZE);
}

const Level3MessageHeader& Level3Message::header() const
{
   return p->header_;
}

void Level3Message::set_header(Level3MessageHeader&& header)
{
   p->header_ = std::move(header);
}

std::shared_ptr<ProductDescriptionBlock>
Level3Message::description_block() const
{
   return nullptr;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
