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

class ScitForecastDataPacketImpl;

class ScitForecastDataPacket : public SpecialGraphicSymbolPacket
{
public:
   explicit ScitForecastDataPacket();
   ~ScitForecastDataPacket();

   ScitForecastDataPacket(const ScitForecastDataPacket&) = delete;
   ScitForecastDataPacket& operator=(const ScitForecastDataPacket&) = delete;

   ScitForecastDataPacket(ScitForecastDataPacket&&) noexcept;
   ScitForecastDataPacket& operator=(ScitForecastDataPacket&&) noexcept;

   const std::vector<uint8_t>& data() const;

   size_t RecordCount() const override;

   static std::shared_ptr<ScitForecastDataPacket> Create(std::istream& is);

protected:
   bool ParseData(std::istream& is) override;

private:
   std::unique_ptr<ScitForecastDataPacketImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
