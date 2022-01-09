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

class MesocycloneSymbolPacketImpl;

class MesocycloneSymbolPacket : public SpecialGraphicSymbolPacket
{
public:
   explicit MesocycloneSymbolPacket();
   ~MesocycloneSymbolPacket();

   MesocycloneSymbolPacket(const MesocycloneSymbolPacket&) = delete;
   MesocycloneSymbolPacket& operator=(const MesocycloneSymbolPacket&) = delete;

   MesocycloneSymbolPacket(MesocycloneSymbolPacket&&) noexcept;
   MesocycloneSymbolPacket& operator=(MesocycloneSymbolPacket&&) noexcept;

   int16_t i_position(size_t i) const;
   int16_t j_position(size_t i) const;
   int16_t radius_of_mesocyclone(size_t i) const;

   size_t RecordCount() const override;

   static std::shared_ptr<MesocycloneSymbolPacket> Create(std::istream& is);

protected:
   bool ParseData(std::istream& is) override;

private:
   std::unique_ptr<MesocycloneSymbolPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
