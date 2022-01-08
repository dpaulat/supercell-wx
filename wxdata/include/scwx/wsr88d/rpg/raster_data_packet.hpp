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

class RasterDataPacketImpl;

class RasterDataPacket : public Packet
{
public:
   explicit RasterDataPacket();
   ~RasterDataPacket();

   RasterDataPacket(const RasterDataPacket&) = delete;
   RasterDataPacket& operator=(const RasterDataPacket&) = delete;

   RasterDataPacket(RasterDataPacket&&) noexcept;
   RasterDataPacket& operator=(RasterDataPacket&&) noexcept;

   uint16_t packet_code() const;
   uint16_t op_flag(size_t i) const;
   int16_t  i_coordinate_start() const;
   int16_t  j_coordinate_start() const;
   uint16_t x_scale_int() const;
   uint16_t x_scale_fractional() const;
   uint16_t y_scale_int() const;
   uint16_t y_scale_fractional() const;
   uint16_t number_of_rows() const;
   uint16_t packaging_descriptor() const;

   size_t data_size() const override;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<RasterDataPacket> Create(std::istream& is);

private:
   std::unique_ptr<RasterDataPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
