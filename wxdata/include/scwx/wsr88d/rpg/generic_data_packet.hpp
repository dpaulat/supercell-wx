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

class GenericDataPacketImpl;

class GenericDataPacket : public Packet
{
public:
   explicit GenericDataPacket();
   ~GenericDataPacket();

   GenericDataPacket(const GenericDataPacket&) = delete;
   GenericDataPacket& operator=(const GenericDataPacket&) = delete;

   GenericDataPacket(GenericDataPacket&&) noexcept;
   GenericDataPacket& operator=(GenericDataPacket&&) noexcept;

   uint16_t packet_code() const override;
   uint32_t length_of_block() const;

   size_t data_size() const override;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<GenericDataPacket> Create(std::istream& is);

private:
   std::unique_ptr<GenericDataPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
