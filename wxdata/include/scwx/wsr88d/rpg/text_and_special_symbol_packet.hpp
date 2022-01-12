#pragma once

#include <scwx/wsr88d/rpg/packet.hpp>

#include <cstdint>
#include <memory>
#include <string>

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
   int16_t                 start_i() const;
   int16_t                 start_j() const;
   std::string             text() const;

   size_t data_size() const override;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<TextAndSpecialSymbolPacket> Create(std::istream& is);

private:
   std::unique_ptr<TextAndSpecialSymbolPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
