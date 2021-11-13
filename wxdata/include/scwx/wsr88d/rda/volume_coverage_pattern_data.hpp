#pragma once

#include <scwx/wsr88d/rda/message.hpp>

#include <string>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

enum class WaveformType
{
   ContiguousSurveillance,
   ContiguousDopplerWithAmbiguityResolution,
   ContiguousDopplerWithoutAmbiguityResolution,
   Batch,
   StaggeredPulsePair,
   Unknown
};

class VolumeCoveragePatternDataImpl;

class VolumeCoveragePatternData : public Message
{
public:
   explicit VolumeCoveragePatternData();
   ~VolumeCoveragePatternData();

   VolumeCoveragePatternData(const VolumeCoveragePatternData&) = delete;
   VolumeCoveragePatternData&
   operator=(const VolumeCoveragePatternData&) = delete;

   VolumeCoveragePatternData(VolumeCoveragePatternData&&) noexcept;
   VolumeCoveragePatternData& operator=(VolumeCoveragePatternData&&) noexcept;

   uint16_t     pattern_type() const;
   uint16_t     pattern_number() const;
   uint16_t     number_of_elevation_cuts() const;
   uint8_t      version() const;
   uint8_t      clutter_map_group_number() const;
   float        doppler_velocity_resolution() const;
   uint8_t      pulse_width() const;
   uint16_t     vcp_sequencing() const;
   uint16_t     number_of_elevations() const;
   uint16_t     maximum_sails_cuts() const;
   bool         sequence_active() const;
   bool         truncated_vcp() const;
   uint16_t     vcp_supplemental_data() const;
   bool         sails_vcp() const;
   uint16_t     number_of_sails_cuts() const;
   bool         mrle_vcp() const;
   uint16_t     number_of_mrle_cuts() const;
   bool         mpda_vcp() const;
   bool         base_tilt_vcp() const;
   uint16_t     number_of_base_tilts() const;
   double       elevation_angle(uint16_t e) const;
   uint16_t     elevation_angle_raw(uint16_t e) const;
   uint8_t      channel_configuration(uint16_t e) const;
   WaveformType waveform_type(uint16_t e) const;
   uint8_t      waveform_type_raw(uint16_t e) const;
   uint8_t      super_resolution_control(uint16_t e) const;
   bool         half_degree_azimuth(uint16_t e) const;
   bool         quarter_km_reflectivity(uint16_t e) const;
   bool         doppler_to_300km(uint16_t e) const;
   bool         dual_polarization_to_300km(uint16_t e) const;
   uint8_t      surveillance_prf_number(uint16_t e) const;
   uint16_t     surveillance_prf_pulse_count_radial(uint16_t e) const;
   double       azimuth_rate(uint16_t e) const;
   float        reflectivity_threshold(uint16_t e) const;
   float        velocity_threshold(uint16_t e) const;
   float        spectrum_width_threshold(uint16_t e) const;
   float        differential_reflectivity_threshold(uint16_t e) const;
   float        differential_phase_threshold(uint16_t e) const;
   float        correlation_coefficient_threshold(uint16_t e) const;
   uint16_t     supplemental_data(uint16_t e) const;
   bool         sails_cut(uint16_t e) const;
   uint16_t     sails_sequence_number(uint16_t e) const;
   bool         mrle_cut(uint16_t e) const;
   uint16_t     mrle_sequence_number(uint16_t e) const;
   bool         mpda_cut(uint16_t e) const;
   bool         base_tilt_cut(uint16_t e) const;
   double       ebc_angle(uint16_t e) const;
   double       edge_angle(uint16_t e, uint16_t s) const;
   uint16_t     doppler_prf_number(uint16_t e, uint16_t s) const;
   uint16_t     doppler_prf_pulse_count_radial(uint16_t e, uint16_t s) const;

   bool Parse(std::istream& is);

   static std::shared_ptr<VolumeCoveragePatternData>
   Create(MessageHeader&& header, std::istream& is);

private:
   std::unique_ptr<VolumeCoveragePatternDataImpl> p;
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
