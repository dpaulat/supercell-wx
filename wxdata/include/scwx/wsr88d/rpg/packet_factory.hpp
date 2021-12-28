#pragma once

#include <scwx/wsr88d/rpg/packet.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class PacketFactory
{
private:
   explicit PacketFactory() = delete;
   ~PacketFactory()         = delete;

   PacketFactory(const PacketFactory&) = delete;
   PacketFactory& operator=(const PacketFactory&) = delete;

   PacketFactory(PacketFactory&&) noexcept = delete;
   PacketFactory& operator=(PacketFactory&&) noexcept = delete;

public:
   static std::shared_ptr<Packet> Create(std::istream& is);
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
