#pragma once

#include <scwx/wsr88d/rpg/packet.hpp>
#include <scwx/wsr88d/rpg/rpg_types.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

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

   std::uint16_t                packet_code() const override;
   std::uint16_t                length_of_block() const;
   std::optional<std::uint16_t> value_of_text() const;
   std::int16_t                 start_i() const;
   std::int16_t                 start_j() const;
   std::string                  text() const;
   SpecialSymbol                special_symbol() const;

   std::size_t data_size() const override;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<TextAndSpecialSymbolPacket> Create(std::istream& is);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
