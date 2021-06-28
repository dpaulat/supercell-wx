#pragma once

#include <scwx/wsr88d/rda/message.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

class RdaAdaptationDataImpl;

class RdaAdaptationData : public Message
{
public:
   explicit RdaAdaptationData();
   ~RdaAdaptationData();

   RdaAdaptationData(const RdaAdaptationData&) = delete;
   RdaAdaptationData& operator=(const RdaAdaptationData&) = delete;

   RdaAdaptationData(RdaAdaptationData&&) noexcept;
   RdaAdaptationData& operator=(RdaAdaptationData&&) noexcept;

   const std::string& adap_file_name() const;
   const std::string& adap_format() const;
   const std::string& adap_revision() const;
   const std::string& adap_date() const;
   const std::string& adap_time() const;
   float              lower_pre_limit() const;
   float              az_lat() const;
   float              upper_pre_limit() const;
   float              el_lat() const;
   float              parkaz() const;
   float              parkel() const;
   float              a_fuel_conv(unsigned i) const;
   float              a_min_shelter_temp() const;
   float              a_max_shelter_temp() const;
   float              a_min_shelter_ac_temp_diff() const;
   float              a_max_xmtr_air_temp() const;
   float              a_max_rad_temp() const;
   float              a_max_rad_temp_rise() const;
   float              lower_dead_limit() const;
   float              upper_dead_limit() const;
   float              a_min_gen_room_temp() const;
   float              a_max_gen_room_temp() const;
   float              spip_5v_reg_lim() const;
   float              spip_15v_reg_lim() const;
   bool               rpg_co_located() const;
   bool               spec_filter_installed() const;
   bool               tps_installed() const;
   bool               rms_installed() const;
   uint32_t           a_hvdl_tst_int() const;
   uint32_t           a_rpg_lt_int() const;
   uint32_t           a_min_stab_util_pwr_time() const;
   uint32_t           a_gen_auto_exer_interval() const;
   uint32_t           a_util_pwr_sw_req_interval() const;
   float              a_low_fuel_level() const;
   uint32_t           config_chan_number() const;
   uint32_t           redundant_chan_config() const;
   float              atten_table(unsigned i) const;
   float              path_losses(unsigned i) const;
   float              h_coupler_xmt_loss() const;
   float              h_coupler_cw_loss() const;
   float              v_coupler_xmt_loss() const;
   float              ame_ts_bias() const;
   float              v_coupler_cw_loss() const;
   float              pwr_sense_bias() const;
   float              ame_v_noise_enr() const;
   float              chan_cal_diff() const;
   float              v_ts_cw() const;
   float              h_rnscale(unsigned i) const;
   float              atmos(unsigned i) const;
   float              el_index(unsigned i) const;
   uint32_t           tfreq_mhz() const;
   float              base_data_tcn() const;
   float              refl_data_tover() const;
   float              tar_h_dbz0_lp() const;
   float              tar_v_dbz0_lp() const;
   uint32_t           init_phi_dp() const;
   uint32_t           norm_init_phi_dp() const;
   float              lx_lp() const;
   float              lx_sp() const;
   float              meteor_param() const;
   float              antenna_gain() const;
   float              vel_degrad_limit() const;
   float              wth_degrad_limit() const;
   float              h_noisetemp_dgrad_limit() const;
   uint32_t           h_min_noisetemp() const;
   float              v_noisetemp_dgrad_limit() const;
   uint32_t           v_min_noisetemp() const;
   float              kly_degrade_limit() const;
   float              ts_coho() const;
   float              h_ts_cw() const;
   float              ts_stalo() const;
   float              ame_h_noise_enr() const;
   float              xmtr_peak_pwr_high_limit() const;
   float              xmtr_peak_pwr_low_limit() const;
   float              h_dbz0_delta_limit() const;
   float              threshold1() const;
   float              threshold2() const;
   float              clut_supp_dgrad_lim() const;
   float              range0_value() const;
   float              xmtr_pwr_mtr_scale() const;
   float              v_dbz0_delta_limit() const;
   float              tar_h_dbz0_sp() const;
   float              tar_v_dbz0_sp() const;
   uint32_t           deltaprf() const;
   uint32_t           tau_sp() const;
   uint32_t           tau_lp() const;
   uint32_t           nc_dead_value() const;
   uint32_t           tau_rf_sp() const;
   uint32_t           tau_rf_lp() const;
   float              seg1_lim() const;
   float              slatsec() const;
   float              slonsec() const;
   uint32_t           slatdeg() const;
   uint32_t           slatmin() const;
   uint32_t           slondeg() const;
   uint32_t           slonmin() const;
   char               slatdir() const;
   char               slondir() const;
   float              az_correction_factor() const;
   float              el_correction_factor() const;
   const std::string& site_name() const;
   float              ant_manual_setup_ielmin() const;
   float              ant_manual_setup_ielmax() const;
   uint32_t           ant_manual_setup_fazvelmax() const;
   uint32_t           ant_manual_setup_felvelmax() const;
   int32_t            ant_manual_setup_ignd_hgt() const;
   uint32_t           ant_manual_setup_irad_hgt() const;
   float              az_pos_sustain_drive() const;
   float              az_neg_sustain_drive() const;
   float              az_nom_pos_drive_slope() const;
   float              az_nom_neg_drive_slope() const;
   float              az_feedback_slope() const;
   float              el_pos_sustain_drive() const;
   float              el_neg_sustain_drive() const;
   float              el_nom_pos_drive_slope() const;
   float              el_nom_neg_drive_slope() const;
   float              el_feedback_slope() const;
   float              el_first_slope() const;
   float              el_second_slope() const;
   float              el_third_slope() const;
   float              el_droop_pos() const;
   float              el_off_neutral_drive() const;
   float              az_intertia() const;
   float              el_inertia() const;
   uint32_t           rvp8nv_iwaveguide_length() const;
   float              v_rnscale(unsigned i) const;
   float              vel_data_tover() const;
   float              width_data_tover() const;
   float              doppler_range_start() const;
   uint32_t           max_el_index() const;
   float              seg2_lim() const;
   float              seg3_lim() const;
   float              seg4_lim() const;
   uint32_t           nbr_el_segments() const;
   float              h_noise_long() const;
   float              ant_noise_temp() const;
   float              h_noise_short() const;
   float              h_noise_tolerance() const;
   float              min_h_dyn_range() const;
   bool               gen_installed() const;
   bool               gen_exercise() const;
   float              v_noise_tolerance() const;
   float              min_v_dyn_range() const;
   float              zdr_bias_dgrad_lim() const;
   float              baseline_zdr_bias() const;
   float              v_noise_long() const;
   float              v_noise_short() const;
   float              zdr_data_tover() const;
   float              phi_data_tover() const;
   float              rho_data_tover() const;
   float              stalo_power_dgrad_limit() const;
   float              stalo_power_maint_limit() const;
   float              min_h_pwr_sense() const;
   float              min_v_pwr_sense() const;
   float              h_pwr_sense_offset() const;
   float              v_pwr_sense_offset() const;
   float              ps_gain_ref() const;
   float              rf_pallet_broad_loss() const;
   float              ame_ps_tolerance() const;
   float              ame_max_temp() const;
   float              ame_min_temp() const;
   float              rcvr_mod_max_temp() const;
   float              rcvr_mod_min_temp() const;
   float              bite_mod_max_temp() const;
   float              bite_mod_min_temp() const;
   uint32_t           default_polarization() const;
   float              tr_limit_dgrad_limit() const;
   float              tr_limit_fail_limit() const;
   bool               rfp_stepper_enabled() const;
   float              ame_current_tolerance() const;
   uint32_t           h_only_polarization() const;
   uint32_t           v_only_polarization() const;
   float              sun_bias() const;
   float              a_min_shelter_temp_warn() const;
   float              power_meter_zero() const;
   float              txb_baseline() const;
   float              txb_alarm_thresh() const;

   bool Parse(std::istream& is);

   static std::shared_ptr<RdaAdaptationData> Create(MessageHeader&& header,
                                                    std::istream&   is);

private:
   std::unique_ptr<RdaAdaptationDataImpl> p;
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
