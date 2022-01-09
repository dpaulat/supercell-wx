#pragma once

#include <scwx/wsr88d/rpg/packet.hpp>

#include <cstdint>
#include <memory>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class WindBarbDataPacketImpl;

class WindBarbDataPacket : public Packet
{
public:
   explicit WindBarbDataPacket();
   ~WindBarbDataPacket();

   WindBarbDataPacket(const WindBarbDataPacket&) = delete;
   WindBarbDataPacket& operator=(const WindBarbDataPacket&) = delete;

   WindBarbDataPacket(WindBarbDataPacket&&) noexcept;
   WindBarbDataPacket& operator=(WindBarbDataPacket&&) noexcept;

   uint16_t packet_code() const;
   uint16_t length_of_block() const;

   size_t data_size() const override;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<WindBarbDataPacket> Create(std::istream& is);

private:
   std::unique_ptr<WindBarbDataPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
