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

class SpecialGraphicSymbolPacketImpl;

class SpecialGraphicSymbolPacket : public Packet
{
public:
   explicit SpecialGraphicSymbolPacket();
   ~SpecialGraphicSymbolPacket();

   SpecialGraphicSymbolPacket(const SpecialGraphicSymbolPacket&) = delete;
   SpecialGraphicSymbolPacket&
   operator=(const SpecialGraphicSymbolPacket&) = delete;

   SpecialGraphicSymbolPacket(SpecialGraphicSymbolPacket&&) noexcept;
   SpecialGraphicSymbolPacket& operator=(SpecialGraphicSymbolPacket&&) noexcept;

   uint16_t packet_code() const override;
   uint16_t length_of_block() const;

   size_t data_size() const override;

   bool Parse(std::istream& is) override;

   virtual size_t RecordCount() const = 0;

protected:
   virtual size_t MinBlockLength() const;
   virtual size_t MaxBlockLength() const;

   virtual bool ParseData(std::istream& is) = 0;

private:
   std::unique_ptr<SpecialGraphicSymbolPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
