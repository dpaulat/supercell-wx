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

class HdaHailSymbolPacketImpl;

class HdaHailSymbolPacket : public SpecialGraphicSymbolPacket
{
public:
   explicit HdaHailSymbolPacket();
   ~HdaHailSymbolPacket();

   HdaHailSymbolPacket(const HdaHailSymbolPacket&) = delete;
   HdaHailSymbolPacket& operator=(const HdaHailSymbolPacket&) = delete;

   HdaHailSymbolPacket(HdaHailSymbolPacket&&) noexcept;
   HdaHailSymbolPacket& operator=(HdaHailSymbolPacket&&) noexcept;

   int16_t  i_position(size_t i) const;
   int16_t  j_position(size_t i) const;
   int16_t  probability_of_hail(size_t i) const;
   int16_t  probability_of_severe_hail(size_t i) const;
   uint16_t max_hail_size(size_t i) const;

   size_t RecordCount() const override;

   static std::shared_ptr<HdaHailSymbolPacket> Create(std::istream& is);

protected:
   bool ParseData(std::istream& is) override;

private:
   std::unique_ptr<HdaHailSymbolPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
