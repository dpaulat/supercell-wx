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

class CellTrendVolumeScanTimesImpl;

class CellTrendVolumeScanTimes : public Packet
{
public:
   explicit CellTrendVolumeScanTimes();
   ~CellTrendVolumeScanTimes();

   CellTrendVolumeScanTimes(const CellTrendVolumeScanTimes&) = delete;
   CellTrendVolumeScanTimes&
   operator=(const CellTrendVolumeScanTimes&) = delete;

   CellTrendVolumeScanTimes(CellTrendVolumeScanTimes&&) noexcept;
   CellTrendVolumeScanTimes& operator=(CellTrendVolumeScanTimes&&) noexcept;

   uint16_t packet_code() const;
   uint16_t length_of_block() const;
   uint16_t number_of_volumes() const;
   uint16_t latest_volume_pointer() const;
   uint16_t volume_time(uint16_t v) const;

   size_t data_size() const override;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<CellTrendVolumeScanTimes> Create(std::istream& is);

private:
   std::unique_ptr<CellTrendVolumeScanTimesImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
