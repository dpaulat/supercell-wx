#pragma once

#include <scwx/wsr88d/rpg/generic_radial_data_packet.hpp>

#include <cstdint>
#include <memory>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class DigitalRadialDataArrayPacketImpl;

class DigitalRadialDataArrayPacket : public GenericRadialDataPacket
{
public:
   explicit DigitalRadialDataArrayPacket();
   ~DigitalRadialDataArrayPacket();

   DigitalRadialDataArrayPacket(const DigitalRadialDataArrayPacket&) = delete;
   DigitalRadialDataArrayPacket&
   operator=(const DigitalRadialDataArrayPacket&) = delete;

   DigitalRadialDataArrayPacket(DigitalRadialDataArrayPacket&&) noexcept;
   DigitalRadialDataArrayPacket&
   operator=(DigitalRadialDataArrayPacket&&) noexcept;

   uint16_t packet_code() const;
   uint16_t index_of_first_range_bin() const;
   uint16_t number_of_range_bins() const;
   int16_t  i_center_of_sweep() const;
   int16_t  j_center_of_sweep() const;
   float    range_scale_factor() const;
   uint16_t number_of_radials() const;

   float                       start_angle(uint16_t r) const;
   float                       delta_angle(uint16_t r) const;
   const std::vector<uint8_t>& level(uint16_t r) const;

   size_t data_size() const override;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<DigitalRadialDataArrayPacket>
   Create(std::istream& is);

private:
   std::unique_ptr<DigitalRadialDataArrayPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
