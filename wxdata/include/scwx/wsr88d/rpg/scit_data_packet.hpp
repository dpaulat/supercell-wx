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

class ScitDataPacket : public SpecialGraphicSymbolPacket
{
public:
   explicit ScitDataPacket();
   ~ScitDataPacket();

   ScitDataPacket(const ScitDataPacket&)            = delete;
   ScitDataPacket& operator=(const ScitDataPacket&) = delete;

   ScitDataPacket(ScitDataPacket&&) noexcept;
   ScitDataPacket& operator=(ScitDataPacket&&) noexcept;

   const std::vector<std::uint8_t>& data() const;

   std::size_t RecordCount() const override;

   static std::shared_ptr<ScitDataPacket> Create(std::istream& is);

protected:
   bool ParseData(std::istream& is) override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
