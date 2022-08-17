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

class PrecipitationRateDataArrayPacketImpl;

class PrecipitationRateDataArrayPacket : public Packet
{
public:
   explicit PrecipitationRateDataArrayPacket();
   ~PrecipitationRateDataArrayPacket();

   PrecipitationRateDataArrayPacket(
      const PrecipitationRateDataArrayPacket&) = delete;
   PrecipitationRateDataArrayPacket&
   operator=(const PrecipitationRateDataArrayPacket&) = delete;

   PrecipitationRateDataArrayPacket(
      PrecipitationRateDataArrayPacket&&) noexcept;
   PrecipitationRateDataArrayPacket&
   operator=(PrecipitationRateDataArrayPacket&&) noexcept;

   uint16_t packet_code() const override;
   uint16_t number_of_lfm_boxes_in_row() const;
   uint16_t number_of_rows() const;

   size_t data_size() const override;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<PrecipitationRateDataArrayPacket>
   Create(std::istream& is);

private:
   std::unique_ptr<PrecipitationRateDataArrayPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
