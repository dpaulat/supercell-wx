#pragma once

#include <scwx/wsr88d/rda/message.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

class DigitalRadarDataImpl;

class DigitalRadarData : public Message
{
public:
   explicit DigitalRadarData();
   ~DigitalRadarData();

   DigitalRadarData(const Message&) = delete;
   DigitalRadarData& operator=(const DigitalRadarData&) = delete;

   DigitalRadarData(DigitalRadarData&&) noexcept;
   DigitalRadarData& operator=(DigitalRadarData&&) noexcept;

   const std::string& radar_identifier() const;
   uint32_t           collection_time() const;
   uint16_t           modified_julian_date() const;
   uint16_t           azimuth_number() const;
   float              azimuth_angle() const;
   uint8_t            compression_indicator() const;
   uint16_t           radial_length() const;
   uint8_t            azimuth_resolution_spacing() const;
   uint8_t            radial_status() const;
   uint8_t            elevation_number() const;
   uint8_t            cut_sector_number() const;
   float              elevation_angle() const;
   uint8_t            radial_spot_blanking_status() const;
   uint8_t            azimuth_indexing_mode() const;
   uint16_t           data_block_count() const;

   bool Parse(std::istream& is);

   static std::shared_ptr<DigitalRadarData> Create(MessageHeader&& header,
                                                   std::istream&   is);

private:
   std::unique_ptr<DigitalRadarDataImpl> p;
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
