#pragma once

#include <scwx/wsr88d/rpg/level3_message.hpp>

#include <cstdint>
#include <memory>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

class GeneralStatusMessageImpl;

class GeneralStatusMessage : public Level3Message
{
public:
   explicit GeneralStatusMessage();
   ~GeneralStatusMessage();

   GeneralStatusMessage(const GeneralStatusMessage&) = delete;
   GeneralStatusMessage& operator=(const GeneralStatusMessage&) = delete;

   GeneralStatusMessage(GeneralStatusMessage&&) noexcept;
   GeneralStatusMessage& operator=(GeneralStatusMessage&&) noexcept;

   int16_t  block_divider() const;
   uint16_t length_of_block() const;
   uint16_t mode_of_operation() const;
   uint16_t rda_operability_status() const;
   uint16_t volume_coverage_pattern() const;
   uint16_t number_of_elevation_cuts() const;
   float    elevation(uint16_t e) const;
   uint16_t rda_status() const;
   uint16_t rda_alarms() const;
   uint16_t data_transmission_enabled() const;
   uint16_t rpg_operability_status() const;
   uint16_t rpg_alarms() const;
   uint16_t rpg_status() const;
   uint16_t rpg_narrowband_status() const;
   float    horizontal_reflectivity_calibration_correction() const;
   uint16_t product_availiability() const;
   uint16_t super_resolution_elevation_cuts() const;
   uint16_t clutter_mitigation_decision_status() const;
   float    vertical_reflectivity_calibration_correction() const;
   uint16_t rda_build_number() const;
   uint16_t rda_channel_number() const;
   uint16_t build_version() const;
   uint16_t vcp_supplemental_data() const;
   uint32_t supplemental_cut_map() const;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<GeneralStatusMessage>
   Create(Level3MessageHeader&& header, std::istream& is);

private:
   std::unique_ptr<GeneralStatusMessageImpl> p;
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
