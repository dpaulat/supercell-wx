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

class RadialDataPacketImpl;

class RadialDataPacket : public Packet
{
public:
   explicit RadialDataPacket();
   ~RadialDataPacket();

   RadialDataPacket(const RadialDataPacket&) = delete;
   RadialDataPacket& operator=(const RadialDataPacket&) = delete;

   RadialDataPacket(RadialDataPacket&&) noexcept;
   RadialDataPacket& operator=(RadialDataPacket&&) noexcept;

   uint16_t packet_code() const;
   uint16_t index_of_first_range_bin() const;
   uint16_t number_of_range_bins() const;
   int16_t  i_center_of_sweep() const;
   int16_t  j_center_of_sweep() const;
   float    scale_factor() const;
   uint16_t number_of_radials() const;

   size_t data_size() const override;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<RadialDataPacket> Create(std::istream& is);

private:
   std::unique_ptr<RadialDataPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
