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

class DigitalPrecipitationDataArrayPacketImpl;

class DigitalPrecipitationDataArrayPacket : public Packet
{
public:
   explicit DigitalPrecipitationDataArrayPacket();
   ~DigitalPrecipitationDataArrayPacket();

   DigitalPrecipitationDataArrayPacket(
      const DigitalPrecipitationDataArrayPacket&) = delete;
   DigitalPrecipitationDataArrayPacket&
   operator=(const DigitalPrecipitationDataArrayPacket&) = delete;

   DigitalPrecipitationDataArrayPacket(
      DigitalPrecipitationDataArrayPacket&&) noexcept;
   DigitalPrecipitationDataArrayPacket&
   operator=(DigitalPrecipitationDataArrayPacket&&) noexcept;

   uint16_t packet_code() const;
   uint16_t number_of_lfm_boxes_in_row() const;
   uint16_t number_of_rows() const;

   size_t data_size() const override;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<DigitalPrecipitationDataArrayPacket>
   Create(std::istream& is);

private:
   std::unique_ptr<DigitalPrecipitationDataArrayPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
