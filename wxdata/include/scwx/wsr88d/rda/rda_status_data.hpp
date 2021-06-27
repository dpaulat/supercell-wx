#pragma once

#include <scwx/wsr88d/rda/message.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

class RdaStatusDataImpl;

class RdaStatusData : public Message
{
public:
   explicit RdaStatusData();
   ~RdaStatusData();

   RdaStatusData(const Message&) = delete;
   RdaStatusData& operator=(const RdaStatusData&) = delete;

   RdaStatusData(RdaStatusData&&) noexcept;
   RdaStatusData& operator=(RdaStatusData&&) noexcept;

   uint16_t rda_status() const;
   uint16_t operability_status() const;
   uint16_t control_status() const;
   uint16_t auxiliary_power_generator_state() const;
   uint16_t average_transmitter_power() const;
   float    horizontal_reflectivity_calibration_correction() const;
   uint16_t data_transmission_enabled() const;
   uint16_t volume_coverage_pattern_number() const;
   uint16_t rda_control_authorization() const;
   uint16_t rda_build_number() const;
   uint16_t operational_mode() const;
   uint16_t super_resolution_status() const;
   uint16_t clutter_mitigation_decision_status() const;
   uint16_t avset_ebc_rda_log_data_status() const;
   uint16_t rda_alarm_summary() const;
   uint16_t command_acknowledgement() const;
   uint16_t channel_control_status() const;
   uint16_t spot_blanking_status() const;
   uint16_t bypass_map_generation_date() const;
   uint16_t bypass_map_generation_time() const;
   uint16_t clutter_filter_map_generation_date() const;
   uint16_t clutter_filter_map_generation_time() const;
   float    vertical_reflectivity_calibration_correction() const;
   uint16_t transition_power_source_status() const;
   uint16_t rms_control_status() const;
   uint16_t performance_check_status() const;
   uint16_t alarm_codes(unsigned i) const;
   uint16_t signal_processing_options() const;
   uint16_t status_version() const;

   bool Parse(std::istream& is);

   static std::shared_ptr<RdaStatusData> Create(MessageHeader&& header,
                                                std::istream&   is);

private:
   std::unique_ptr<RdaStatusDataImpl> p;
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
