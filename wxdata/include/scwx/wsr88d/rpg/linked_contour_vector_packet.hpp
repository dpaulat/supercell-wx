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

class LinkedContourVectorPacketImpl;

class LinkedContourVectorPacket : public Packet
{
public:
   explicit LinkedContourVectorPacket();
   ~LinkedContourVectorPacket();

   LinkedContourVectorPacket(const LinkedContourVectorPacket&) = delete;
   LinkedContourVectorPacket&
   operator=(const LinkedContourVectorPacket&) = delete;

   LinkedContourVectorPacket(LinkedContourVectorPacket&&) noexcept;
   LinkedContourVectorPacket& operator=(LinkedContourVectorPacket&&) noexcept;

   uint16_t packet_code() const;
   uint16_t initial_point_indicator() const;
   uint16_t length_of_vectors() const;

   size_t data_size() const override;

   bool Parse(std::istream& is) override;

private:
   std::unique_ptr<LinkedContourVectorPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
