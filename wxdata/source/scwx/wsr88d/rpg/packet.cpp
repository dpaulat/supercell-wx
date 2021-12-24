#include <scwx/wsr88d/rpg/packet.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ = "[scwx::wsr88d::rpg::packet] ";

Packet::Packet()  = default;
Packet::~Packet() = default;

Packet::Packet(Packet&&) noexcept = default;
Packet& Packet::operator=(Packet&&) noexcept = default;

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
