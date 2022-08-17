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

class CellTrendDataPacketImpl;

class CellTrendDataPacket : public Packet
{
public:
   explicit CellTrendDataPacket();
   ~CellTrendDataPacket();

   CellTrendDataPacket(const CellTrendDataPacket&) = delete;
   CellTrendDataPacket& operator=(const CellTrendDataPacket&) = delete;

   CellTrendDataPacket(CellTrendDataPacket&&) noexcept;
   CellTrendDataPacket& operator=(CellTrendDataPacket&&) noexcept;

   uint16_t    packet_code() const override;
   uint16_t    length_of_block() const;
   std::string cell_id() const;
   int16_t     i_position() const;
   int16_t     j_position() const;
   uint16_t    number_of_trends() const;
   uint16_t    trend_code(uint16_t t) const;
   uint8_t     number_of_volumes(uint16_t t) const;
   uint8_t     latest_volume_pointer(uint16_t t) const;
   uint16_t    trend_data(uint16_t t, uint8_t v) const;

   size_t data_size() const override;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<CellTrendDataPacket> Create(std::istream& is);

private:
   std::unique_ptr<CellTrendDataPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
