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

class StormIdSymbolPacketImpl;

class StormIdSymbolPacket : public SpecialGraphicSymbolPacket
{
public:
   explicit StormIdSymbolPacket();
   ~StormIdSymbolPacket();

   StormIdSymbolPacket(const StormIdSymbolPacket&) = delete;
   StormIdSymbolPacket& operator=(const StormIdSymbolPacket&) = delete;

   StormIdSymbolPacket(StormIdSymbolPacket&&) noexcept;
   StormIdSymbolPacket& operator=(StormIdSymbolPacket&&) noexcept;

   int16_t                    i_position(size_t i) const;
   int16_t                    j_position(size_t i) const;
   const std::array<char, 2>& character(size_t i) const;

   size_t RecordCount() const override;

   static std::shared_ptr<StormIdSymbolPacket> Create(std::istream& is);

protected:
   bool ParseData(std::istream& is) override;

private:
   std::unique_ptr<StormIdSymbolPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
