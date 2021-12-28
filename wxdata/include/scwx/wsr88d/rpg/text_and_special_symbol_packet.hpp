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

class TextAndSpecialSymbolPacketImpl;

class TextAndSpecialSymbolPacket : public Packet
{
public:
   explicit TextAndSpecialSymbolPacket();
   ~TextAndSpecialSymbolPacket();

   TextAndSpecialSymbolPacket(const TextAndSpecialSymbolPacket&) = delete;
   TextAndSpecialSymbolPacket&
   operator=(const TextAndSpecialSymbolPacket&) = delete;

   TextAndSpecialSymbolPacket(TextAndSpecialSymbolPacket&&) noexcept;
   TextAndSpecialSymbolPacket& operator=(TextAndSpecialSymbolPacket&&) noexcept;

   uint16_t                packet_code() const;
   uint16_t                length_of_block() const;
   std::optional<uint16_t> value_of_text() const;

   size_t data_size() const override;

   bool Parse(std::istream& is) override;

private:
   std::unique_ptr<TextAndSpecialSymbolPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
