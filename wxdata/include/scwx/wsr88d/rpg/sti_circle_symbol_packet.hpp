#pragma once

#include <scwx/wsr88d/rpg/special_graphic_symbol_packet.hpp>

#include <cstdint>
#include <memory>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class StiCircleSymbolPacketImpl;

class StiCircleSymbolPacket : public SpecialGraphicSymbolPacket
{
public:
   explicit StiCircleSymbolPacket();
   ~StiCircleSymbolPacket();

   StiCircleSymbolPacket(const StiCircleSymbolPacket&) = delete;
   StiCircleSymbolPacket& operator=(const StiCircleSymbolPacket&) = delete;

   StiCircleSymbolPacket(StiCircleSymbolPacket&&) noexcept;
   StiCircleSymbolPacket& operator=(StiCircleSymbolPacket&&) noexcept;

   int16_t  i_position(size_t i) const;
   int16_t  j_position(size_t i) const;
   uint16_t radius_of_circle(size_t i) const;

   size_t RecordCount() const override;

   static std::shared_ptr<StiCircleSymbolPacket> Create(std::istream& is);

protected:
   bool ParseData(std::istream& is) override;

private:
   std::unique_ptr<StiCircleSymbolPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
