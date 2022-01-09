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

class PointGraphicSymbolPacketImpl;

class PointGraphicSymbolPacket : public SpecialGraphicSymbolPacket
{
public:
   explicit PointGraphicSymbolPacket();
   ~PointGraphicSymbolPacket();

   PointGraphicSymbolPacket(const PointGraphicSymbolPacket&) = delete;
   PointGraphicSymbolPacket&
   operator=(const PointGraphicSymbolPacket&) = delete;

   PointGraphicSymbolPacket(PointGraphicSymbolPacket&&) noexcept;
   PointGraphicSymbolPacket& operator=(PointGraphicSymbolPacket&&) noexcept;

   int16_t i_position(size_t i) const;
   int16_t j_position(size_t i) const;

   size_t RecordCount() const override;

   static std::shared_ptr<PointGraphicSymbolPacket> Create(std::istream& is);

protected:
   bool ParseData(std::istream& is) override;

private:
   std::unique_ptr<PointGraphicSymbolPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
