#pragma once

#include <scwx/wsr88d/rda/level2_message.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

class DigitalRadarData : public Level2Message
{
public:
   explicit DigitalRadarData();
   ~DigitalRadarData();

   DigitalRadarData(const DigitalRadarData&)            = delete;
   DigitalRadarData& operator=(const DigitalRadarData&) = delete;

   DigitalRadarData(DigitalRadarData&&) noexcept;
   DigitalRadarData& operator=(DigitalRadarData&&) noexcept;

   std::uint32_t collection_time() const;
   std::uint16_t modified_julian_date() const;
   std::uint16_t unambiguous_range() const;
   std::uint16_t azimuth_angle() const;
   std::uint16_t azimuth_number() const;
   std::uint16_t radial_status() const;
   std::uint16_t elevation_angle() const;
   std::uint16_t elevation_number() const;
   std::uint16_t surveillance_range() const;
   std::uint16_t doppler_range() const;
   std::uint16_t surveillance_range_sample_interval() const;
   std::uint16_t doppler_range_sample_interval() const;
   std::uint16_t number_of_surveillance_bins() const;
   std::uint16_t number_of_doppler_bins() const;
   std::uint16_t cut_sector_number() const;
   float         calibration_constant() const;
   std::uint16_t surveillance_pointer() const;
   std::uint16_t velocity_pointer() const;
   std::uint16_t spectral_width_pointer() const;
   std::uint16_t doppler_velocity_resolution() const;
   std::uint16_t volume_coverage_pattern_number() const;
   std::uint16_t nyquist_velocity() const;
   std::uint16_t atmos() const;
   std::uint16_t tover() const;
   std::uint16_t radial_spot_blanking_status() const;

   bool Parse(std::istream& is);

   static std::shared_ptr<DigitalRadarData> Create(Level2MessageHeader&& header,
                                                   std::istream&         is);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
