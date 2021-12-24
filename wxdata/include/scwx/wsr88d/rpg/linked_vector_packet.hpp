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

class LinkedVectorPacketImpl;

class LinkedVectorPacket : public Packet
{
public:
   explicit LinkedVectorPacket();
   ~LinkedVectorPacket();

   LinkedVectorPacket(const LinkedVectorPacket&) = delete;
   LinkedVectorPacket& operator=(const LinkedVectorPacket&) = delete;

   LinkedVectorPacket(LinkedVectorPacket&&) noexcept;
   LinkedVectorPacket& operator=(LinkedVectorPacket&&) noexcept;

   uint16_t packet_code() const;
   uint16_t length_of_block() const;
   uint16_t value_of_vector() const;

   size_t data_size() const override;

   bool Parse(std::istream& is) override;

private:
   std::unique_ptr<LinkedVectorPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
