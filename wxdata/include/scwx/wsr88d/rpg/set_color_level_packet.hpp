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

class SetColorLevelPacketImpl;

class SetColorLevelPacket : public Packet
{
public:
   explicit SetColorLevelPacket();
   ~SetColorLevelPacket();

   SetColorLevelPacket(const SetColorLevelPacket&) = delete;
   SetColorLevelPacket& operator=(const SetColorLevelPacket&) = delete;

   SetColorLevelPacket(SetColorLevelPacket&&) noexcept;
   SetColorLevelPacket& operator=(SetColorLevelPacket&&) noexcept;

   uint16_t packet_code() const;
   uint16_t color_value_indicator() const;
   uint16_t value_of_contour() const;

   size_t data_size() const override;

   bool Parse(std::istream& is) override;

   static constexpr size_t SIZE = 6u;

private:
   std::unique_ptr<SetColorLevelPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
