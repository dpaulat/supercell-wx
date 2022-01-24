#include <scwx/wsr88d/rda/rda_adaptation_data.hpp>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

static const std::string logPrefix_ =
   "[scwx::wsr88d::rda::rda_adaptation_data] ";

struct AntManualSetup
{
   int32_t  ielmin_;
   int32_t  ielmax_;
   uint32_t fazvelmax_;
   uint32_t felvelmax_;
   int32_t  igndHgt_;
   uint32_t iradHgt_;

   AntManualSetup() :
       ielmin_ {0},
       ielmax_ {0},
       fazvelmax_ {0},
       felvelmax_ {0},
       igndHgt_ {0},
       iradHgt_ {0}
   {
   }
};

class RdaAdaptationDataImpl
{
public:
   explicit RdaAdaptationDataImpl() :
       adapFileName_ {},
       adapFormat_ {},
       adapRevision_ {},
       adapDate_ {},
       adapTime_ {},
       lowerPreLimit_ {0.0f},
       azLat_ {0.0f},
       upperPreLimit_ {0.0f},
       elLat_ {0.0f},
       parkaz_ {0.0f},
       parkel_ {0.0f},
       aFuelConv_ {0.0f},
       aMinShelterTemp_ {0.0f},
       aMaxShelterTemp_ {0.0f},
       aMinShelterAcTempDiff_ {0.0f},
       aMaxXmtrAirTemp_ {0.0f},
       aMaxRadTemp_ {0.0f},
       aMaxRadTempRise_ {0.0f},
       lowerDeadLimit_ {0.0f},
       upperDeadLimit_ {0.0f},
       aMinGenRoomTemp_ {0.0f},
       aMaxGenRoomTemp_ {0.0f},
       spip5VRegLim_ {0.0f},
       spip15VRegLim_ {0.0f},
       rpgCoLocated_ {false},
       specFilterInstalled_ {false},
       tpsInstalled_ {false},
       rmsInstalled_ {false},
       aHvdlTstInt_ {0},
       aRpgLtInt_ {0},
       aMinStabUtilPwrTime_ {0},
       aGenAutoExerInterval_ {0},
       aUtilPwrSwReqInterval_ {0},
       aLowFuelLevel_ {0.0f},
       configChanNumber_ {0},
       redundantChanConfig_ {0},
       attenTable_ {0.0f},
       pathLosses_ {},
       vTsCw_ {0.0f},
       hRnscale_ {0.0f},
       atmos_ {0.0f},
       elIndex_ {0.0f},
       tfreqMhz_ {0},
       baseDataTcn_ {0.0f},
       reflDataTover_ {0.0f},
       tarHDbz0Lp_ {0.0f},
       tarVDbz0Lp_ {0.0f},
       initPhiDp_ {0},
       normInitPhiDp_ {0},
       lxLp_ {0.0f},
       lxSp_ {0.0f},
       meteorParam_ {0.0f},
       antennaGain_ {0.0f},
       velDegradLimit_ {0.0f},
       wthDegradLimit_ {0.0f},
       hNoisetempDgradLimit_ {0.0f},
       hMinNoisetemp_ {0},
       vNoisetempDgradLimit_ {0.0f},
       vMinNoisetemp_ {0},
       klyDegradeLimit_ {0.0f},
       tsCoho_ {0.0f},
       hTsCw_ {0.0f},
       tsStalo_ {0.0f},
       ameHNoiseEnr_ {0.0f},
       xmtrPeakPwrHighLimit_ {0.0f},
       xmtrPeakPwrLowLimit_ {0.0f},
       hDbz0DeltaLimit_ {0.0f},
       threshold1_ {0.0f},
       threshold2_ {0.0f},
       clutSuppDgradLim_ {0.0f},
       range0Value_ {0.0f},
       xmtrPwrMtrScale_ {0.0f},
       vDbz0DeltaLimit_ {0.0f},
       tarHDbz0Sp_ {0.0f},
       tarVDbz0Sp_ {0.0f},
       deltaprf_ {0},
       tauSp_ {0},
       tauLp_ {0},
       ncDeadValue_ {0},
       tauRfSp_ {0},
       tauRfLp_ {0},
       seg1Lim_ {0.0f},
       slatsec_ {0.0f},
       slonsec_ {0.0f},
       slatdeg_ {0},
       slatmin_ {0},
       slondeg_ {0},
       slonmin_ {0},
       slatdir_ {0},
       slondir_ {0},
       azCorrectionFactor_ {0.0f},
       elCorrectionFactor_ {0.0f},
       siteName_ {},
       antManualSetup_(),
       azPosSustainDrive_ {0.0f},
       azNegSustainDrive_ {0.0f},
       azNomPosDriveSlope_ {0.0f},
       azNomNegDriveSlope_ {0.0f},
       azFeedbackSlope_ {0.0f},
       elPosSustainDrive_ {0.0f},
       elNegSustainDrive_ {0.0f},
       elNomPosDriveSlope_ {0.0f},
       elNomNegDriveSlope_ {0.0f},
       elFeedbackSlope_ {0.0f},
       elFirstSlope_ {0.0f},
       elSecondSlope_ {0.0f},
       elThirdSlope_ {0.0f},
       elDroopPos_ {0.0f},
       elOffNeutralDrive_ {0.0f},
       azIntertia_ {0.0f},
       elInertia_ {0.0f},
       rvp8nvIwaveguideLength_ {0},
       vRnscale_ {0.0f},
       velDataTover_ {0.0f},
       widthDataTover_ {0.0f},
       dopplerRangeStart_ {0.0f},
       maxElIndex_ {0},
       seg2Lim_ {0.0f},
       seg3Lim_ {0.0f},
       seg4Lim_ {0.0f},
       nbrElSegments_ {0},
       hNoiseLong_ {0.0f},
       antNoiseTemp_ {0.0f},
       hNoiseShort_ {0.0f},
       hNoiseTolerance_ {0.0f},
       minHDynRange_ {0.0f},
       genInstalled_ {false},
       genExercise_ {false},
       vNoiseTolerance_ {0.0f},
       minVDynRange_ {0.0f},
       zdrBiasDgradLim_ {0.0f},
       baselineZdrBias_ {0.0f},
       vNoiseLong_ {0.0f},
       vNoiseShort_ {0.0f},
       zdrDataTover_ {0.0f},
       phiDataTover_ {0.0f},
       rhoDataTover_ {0.0f},
       staloPowerDgradLimit_ {0.0f},
       staloPowerMaintLimit_ {0.0f},
       minHPwrSense_ {0.0f},
       minVPwrSense_ {0.0f},
       hPwrSenseOffset_ {0.0f},
       vPwrSenseOffset_ {0.0f},
       psGainRef_ {0.0f},
       rfPalletBroadLoss_ {0.0f},
       amePsTolerance_ {0.0f},
       ameMaxTemp_ {0.0f},
       ameMinTemp_ {0.0f},
       rcvrModMaxTemp_ {0.0f},
       rcvrModMinTemp_ {0.0f},
       biteModMaxTemp_ {0.0f},
       biteModMinTemp_ {0.0f},
       defaultPolarization_ {0},
       trLimitDgradLimit_ {0.0f},
       trLimitFailLimit_ {0.0f},
       rfpStepperEnabled_ {false},
       ameCurrentTolerance_ {0.0f},
       hOnlyPolarization_ {0},
       vOnlyPolarization_ {0},
       sunBias_ {0.0f},
       aMinShelterTempWarn_ {0.0f},
       powerMeterZero_ {0.0f},
       txbBaseline_ {0.0f},
       txbAlarmThresh_ {0.0f} {};
   ~RdaAdaptationDataImpl() = default;

   std::string               adapFileName_;
   std::string               adapFormat_;
   std::string               adapRevision_;
   std::string               adapDate_;
   std::string               adapTime_;
   float                     lowerPreLimit_;
   float                     azLat_;
   float                     upperPreLimit_;
   float                     elLat_;
   float                     parkaz_;
   float                     parkel_;
   std::array<float, 11>     aFuelConv_;
   float                     aMinShelterTemp_;
   float                     aMaxShelterTemp_;
   float                     aMinShelterAcTempDiff_;
   float                     aMaxXmtrAirTemp_;
   float                     aMaxRadTemp_;
   float                     aMaxRadTempRise_;
   float                     lowerDeadLimit_;
   float                     upperDeadLimit_;
   float                     aMinGenRoomTemp_;
   float                     aMaxGenRoomTemp_;
   float                     spip5VRegLim_;
   float                     spip15VRegLim_;
   bool                      rpgCoLocated_;
   bool                      specFilterInstalled_;
   bool                      tpsInstalled_;
   bool                      rmsInstalled_;
   uint32_t                  aHvdlTstInt_;
   uint32_t                  aRpgLtInt_;
   uint32_t                  aMinStabUtilPwrTime_;
   uint32_t                  aGenAutoExerInterval_;
   uint32_t                  aUtilPwrSwReqInterval_;
   float                     aLowFuelLevel_;
   uint32_t                  configChanNumber_;
   uint32_t                  redundantChanConfig_;
   std::array<float, 104>    attenTable_;
   std::map<unsigned, float> pathLosses_;
   float                     vTsCw_;
   std::array<float, 13>     hRnscale_;
   std::array<float, 13>     atmos_;
   std::array<float, 12>     elIndex_;
   uint32_t                  tfreqMhz_;
   float                     baseDataTcn_;
   float                     reflDataTover_;
   float                     tarHDbz0Lp_;
   float                     tarVDbz0Lp_;
   uint32_t                  initPhiDp_;
   uint32_t                  normInitPhiDp_;
   float                     lxLp_;
   float                     lxSp_;
   float                     meteorParam_;
   float                     antennaGain_;
   float                     velDegradLimit_;
   float                     wthDegradLimit_;
   float                     hNoisetempDgradLimit_;
   uint32_t                  hMinNoisetemp_;
   float                     vNoisetempDgradLimit_;
   uint32_t                  vMinNoisetemp_;
   float                     klyDegradeLimit_;
   float                     tsCoho_;
   float                     hTsCw_;
   float                     tsStalo_;
   float                     ameHNoiseEnr_;
   float                     xmtrPeakPwrHighLimit_;
   float                     xmtrPeakPwrLowLimit_;
   float                     hDbz0DeltaLimit_;
   float                     threshold1_;
   float                     threshold2_;
   float                     clutSuppDgradLim_;
   float                     range0Value_;
   float                     xmtrPwrMtrScale_;
   float                     vDbz0DeltaLimit_;
   float                     tarHDbz0Sp_;
   float                     tarVDbz0Sp_;
   uint32_t                  deltaprf_;
   uint32_t                  tauSp_;
   uint32_t                  tauLp_;
   uint32_t                  ncDeadValue_;
   uint32_t                  tauRfSp_;
   uint32_t                  tauRfLp_;
   float                     seg1Lim_;
   float                     slatsec_;
   float                     slonsec_;
   uint32_t                  slatdeg_;
   uint32_t                  slatmin_;
   uint32_t                  slondeg_;
   uint32_t                  slonmin_;
   char                      slatdir_;
   char                      slondir_;
   float                     azCorrectionFactor_;
   float                     elCorrectionFactor_;
   std::string               siteName_;
   AntManualSetup            antManualSetup_;
   float                     azPosSustainDrive_;
   float                     azNegSustainDrive_;
   float                     azNomPosDriveSlope_;
   float                     azNomNegDriveSlope_;
   float                     azFeedbackSlope_;
   float                     elPosSustainDrive_;
   float                     elNegSustainDrive_;
   float                     elNomPosDriveSlope_;
   float                     elNomNegDriveSlope_;
   float                     elFeedbackSlope_;
   float                     elFirstSlope_;
   float                     elSecondSlope_;
   float                     elThirdSlope_;
   float                     elDroopPos_;
   float                     elOffNeutralDrive_;
   float                     azIntertia_;
   float                     elInertia_;
   uint32_t                  rvp8nvIwaveguideLength_;
   std::array<float, 13>     vRnscale_;
   float                     velDataTover_;
   float                     widthDataTover_;
   float                     dopplerRangeStart_;
   uint32_t                  maxElIndex_;
   float                     seg2Lim_;
   float                     seg3Lim_;
   float                     seg4Lim_;
   uint32_t                  nbrElSegments_;
   float                     hNoiseLong_;
   float                     antNoiseTemp_;
   float                     hNoiseShort_;
   float                     hNoiseTolerance_;
   float                     minHDynRange_;
   bool                      genInstalled_;
   bool                      genExercise_;
   float                     vNoiseTolerance_;
   float                     minVDynRange_;
   float                     zdrBiasDgradLim_;
   float                     baselineZdrBias_;
   float                     vNoiseLong_;
   float                     vNoiseShort_;
   float                     zdrDataTover_;
   float                     phiDataTover_;
   float                     rhoDataTover_;
   float                     staloPowerDgradLimit_;
   float                     staloPowerMaintLimit_;
   float                     minHPwrSense_;
   float                     minVPwrSense_;
   float                     hPwrSenseOffset_;
   float                     vPwrSenseOffset_;
   float                     psGainRef_;
   float                     rfPalletBroadLoss_;
   float                     amePsTolerance_;
   float                     ameMaxTemp_;
   float                     ameMinTemp_;
   float                     rcvrModMaxTemp_;
   float                     rcvrModMinTemp_;
   float                     biteModMaxTemp_;
   float                     biteModMinTemp_;
   uint32_t                  defaultPolarization_;
   float                     trLimitDgradLimit_;
   float                     trLimitFailLimit_;
   bool                      rfpStepperEnabled_;
   float                     ameCurrentTolerance_;
   uint32_t                  hOnlyPolarization_;
   uint32_t                  vOnlyPolarization_;
   float                     sunBias_;
   float                     aMinShelterTempWarn_;
   float                     powerMeterZero_;
   float                     txbBaseline_;
   float                     txbAlarmThresh_;
};

RdaAdaptationData::RdaAdaptationData() :
    Level2Message(), p(std::make_unique<RdaAdaptationDataImpl>())
{
}
RdaAdaptationData::~RdaAdaptationData() = default;

RdaAdaptationData::RdaAdaptationData(RdaAdaptationData&&) noexcept = default;
RdaAdaptationData&
RdaAdaptationData::operator=(RdaAdaptationData&&) noexcept = default;

std::string RdaAdaptationData::adap_file_name() const
{
   return p->adapFileName_;
}

std::string RdaAdaptationData::adap_format() const
{
   return p->adapFormat_;
}

std::string RdaAdaptationData::adap_revision() const
{
   return p->adapRevision_;
}

std::string RdaAdaptationData::adap_date() const
{
   return p->adapDate_;
}

std::string RdaAdaptationData::adap_time() const
{
   return p->adapTime_;
}

float RdaAdaptationData::lower_pre_limit() const
{
   return p->lowerPreLimit_;
}

float RdaAdaptationData::az_lat() const
{
   return p->azLat_;
}

float RdaAdaptationData::upper_pre_limit() const
{
   return p->upperPreLimit_;
}

float RdaAdaptationData::el_lat() const
{
   return p->elLat_;
}

float RdaAdaptationData::parkaz() const
{
   return p->parkaz_;
}

float RdaAdaptationData::parkel() const
{
   return p->parkel_;
}

float RdaAdaptationData::a_fuel_conv(unsigned i) const
{
   return p->aFuelConv_[i];
}

float RdaAdaptationData::a_min_shelter_temp() const
{
   return p->aMinShelterTemp_;
}

float RdaAdaptationData::a_max_shelter_temp() const
{
   return p->aMaxShelterTemp_;
}

float RdaAdaptationData::a_min_shelter_ac_temp_diff() const
{
   return p->aMinShelterAcTempDiff_;
}

float RdaAdaptationData::a_max_xmtr_air_temp() const
{
   return p->aMaxXmtrAirTemp_;
}

float RdaAdaptationData::a_max_rad_temp() const
{
   return p->aMaxRadTemp_;
}

float RdaAdaptationData::a_max_rad_temp_rise() const
{
   return p->aMaxRadTempRise_;
}

float RdaAdaptationData::lower_dead_limit() const
{
   return p->lowerDeadLimit_;
}

float RdaAdaptationData::upper_dead_limit() const
{
   return p->upperDeadLimit_;
}

float RdaAdaptationData::a_min_gen_room_temp() const
{
   return p->aMinGenRoomTemp_;
}

float RdaAdaptationData::a_max_gen_room_temp() const
{
   return p->aMaxGenRoomTemp_;
}

float RdaAdaptationData::spip_5v_reg_lim() const
{
   return p->spip5VRegLim_;
}

float RdaAdaptationData::spip_15v_reg_lim() const
{
   return p->spip15VRegLim_;
}

bool RdaAdaptationData::rpg_co_located() const
{
   return p->rpgCoLocated_;
}

bool RdaAdaptationData::spec_filter_installed() const
{
   return p->specFilterInstalled_;
}

bool RdaAdaptationData::tps_installed() const
{
   return p->tpsInstalled_;
}

bool RdaAdaptationData::rms_installed() const
{
   return p->rmsInstalled_;
}

uint32_t RdaAdaptationData::a_hvdl_tst_int() const
{
   return p->aHvdlTstInt_;
}

uint32_t RdaAdaptationData::a_rpg_lt_int() const
{
   return p->aRpgLtInt_;
}

uint32_t RdaAdaptationData::a_min_stab_util_pwr_time() const
{
   return p->aMinStabUtilPwrTime_;
}

uint32_t RdaAdaptationData::a_gen_auto_exer_interval() const
{
   return p->aGenAutoExerInterval_;
}

uint32_t RdaAdaptationData::a_util_pwr_sw_req_interval() const
{
   return p->aUtilPwrSwReqInterval_;
}

float RdaAdaptationData::a_low_fuel_level() const
{
   return p->aLowFuelLevel_;
}

uint32_t RdaAdaptationData::config_chan_number() const
{
   return p->configChanNumber_;
}

uint32_t RdaAdaptationData::redundant_chan_config() const
{
   return p->redundantChanConfig_;
}

float RdaAdaptationData::atten_table(unsigned i) const
{
   return p->attenTable_[i];
}

float RdaAdaptationData::path_losses(unsigned i) const
{
   return p->pathLosses_.at(i);
}

float RdaAdaptationData::h_coupler_xmt_loss() const
{
   return path_losses(29);
}

float RdaAdaptationData::h_coupler_cw_loss() const
{
   return path_losses(48);
}

float RdaAdaptationData::v_coupler_xmt_loss() const
{
   return path_losses(49);
}

float RdaAdaptationData::ame_ts_bias() const
{
   return path_losses(51);
}

float RdaAdaptationData::v_coupler_cw_loss() const
{
   return path_losses(53);
}

float RdaAdaptationData::pwr_sense_bias() const
{
   return path_losses(56);
}

float RdaAdaptationData::ame_v_noise_enr() const
{
   return path_losses(57);
}

float RdaAdaptationData::chan_cal_diff() const
{
   return path_losses(70);
}

float RdaAdaptationData::v_ts_cw() const
{
   return p->vTsCw_;
}

float RdaAdaptationData::h_rnscale(unsigned i) const
{
   return p->hRnscale_[i];
}

float RdaAdaptationData::atmos(unsigned i) const
{
   return p->atmos_[i];
}

float RdaAdaptationData::el_index(unsigned i) const
{
   return p->elIndex_[i];
}

uint32_t RdaAdaptationData::tfreq_mhz() const
{
   return p->tfreqMhz_;
}

float RdaAdaptationData::base_data_tcn() const
{
   return p->baseDataTcn_;
}

float RdaAdaptationData::refl_data_tover() const
{
   return p->reflDataTover_;
}

float RdaAdaptationData::tar_h_dbz0_lp() const
{
   return p->tarHDbz0Lp_;
}

float RdaAdaptationData::tar_v_dbz0_lp() const
{
   return p->tarVDbz0Lp_;
}

uint32_t RdaAdaptationData::init_phi_dp() const
{
   return p->initPhiDp_;
}

uint32_t RdaAdaptationData::norm_init_phi_dp() const
{
   return p->normInitPhiDp_;
}

float RdaAdaptationData::lx_lp() const
{
   return p->lxLp_;
}

float RdaAdaptationData::lx_sp() const
{
   return p->lxSp_;
}

float RdaAdaptationData::meteor_param() const
{
   return p->meteorParam_;
}

float RdaAdaptationData::antenna_gain() const
{
   return p->antennaGain_;
}

float RdaAdaptationData::vel_degrad_limit() const
{
   return p->velDegradLimit_;
}

float RdaAdaptationData::wth_degrad_limit() const
{
   return p->wthDegradLimit_;
}

float RdaAdaptationData::h_noisetemp_dgrad_limit() const
{
   return p->hNoisetempDgradLimit_;
}

uint32_t RdaAdaptationData::h_min_noisetemp() const
{
   return p->hMinNoisetemp_;
}

float RdaAdaptationData::v_noisetemp_dgrad_limit() const
{
   return p->vNoisetempDgradLimit_;
}

uint32_t RdaAdaptationData::v_min_noisetemp() const
{
   return p->vMinNoisetemp_;
}

float RdaAdaptationData::kly_degrade_limit() const
{
   return p->klyDegradeLimit_;
}

float RdaAdaptationData::ts_coho() const
{
   return p->tsCoho_;
}

float RdaAdaptationData::h_ts_cw() const
{
   return p->hTsCw_;
}

float RdaAdaptationData::ts_stalo() const
{
   return p->tsStalo_;
}

float RdaAdaptationData::ame_h_noise_enr() const
{
   return p->ameHNoiseEnr_;
}

float RdaAdaptationData::xmtr_peak_pwr_high_limit() const
{
   return p->xmtrPeakPwrHighLimit_;
}

float RdaAdaptationData::xmtr_peak_pwr_low_limit() const
{
   return p->xmtrPeakPwrLowLimit_;
}

float RdaAdaptationData::h_dbz0_delta_limit() const
{
   return p->hDbz0DeltaLimit_;
}

float RdaAdaptationData::threshold1() const
{
   return p->threshold1_;
}

float RdaAdaptationData::threshold2() const
{
   return p->threshold2_;
}

float RdaAdaptationData::clut_supp_dgrad_lim() const
{
   return p->clutSuppDgradLim_;
}

float RdaAdaptationData::range0_value() const
{
   return p->range0Value_;
}

float RdaAdaptationData::xmtr_pwr_mtr_scale() const
{
   return p->xmtrPwrMtrScale_;
}

float RdaAdaptationData::v_dbz0_delta_limit() const
{
   return p->vDbz0DeltaLimit_;
}

float RdaAdaptationData::tar_h_dbz0_sp() const
{
   return p->tarHDbz0Sp_;
}

float RdaAdaptationData::tar_v_dbz0_sp() const
{
   return p->tarVDbz0Sp_;
}

uint32_t RdaAdaptationData::deltaprf() const
{
   return p->deltaprf_;
}

uint32_t RdaAdaptationData::tau_sp() const
{
   return p->tauSp_;
}

uint32_t RdaAdaptationData::tau_lp() const
{
   return p->tauLp_;
}

uint32_t RdaAdaptationData::nc_dead_value() const
{
   return p->ncDeadValue_;
}

uint32_t RdaAdaptationData::tau_rf_sp() const
{
   return p->tauRfSp_;
}

uint32_t RdaAdaptationData::tau_rf_lp() const
{
   return p->tauRfLp_;
}

float RdaAdaptationData::seg1_lim() const
{
   return p->seg1Lim_;
}

float RdaAdaptationData::slatsec() const
{
   return p->slatsec_;
}

float RdaAdaptationData::slonsec() const
{
   return p->slonsec_;
}

uint32_t RdaAdaptationData::slatdeg() const
{
   return p->slatdeg_;
}

uint32_t RdaAdaptationData::slatmin() const
{
   return p->slatmin_;
}

uint32_t RdaAdaptationData::slondeg() const
{
   return p->slondeg_;
}

uint32_t RdaAdaptationData::slonmin() const
{
   return p->slonmin_;
}

char RdaAdaptationData::slatdir() const
{
   return p->slatdir_;
}

char RdaAdaptationData::slondir() const
{
   return p->slondir_;
}

float RdaAdaptationData::az_correction_factor() const
{
   return p->azCorrectionFactor_;
}

float RdaAdaptationData::el_correction_factor() const
{
   return p->elCorrectionFactor_;
}

std::string RdaAdaptationData::site_name() const
{
   return p->siteName_;
}

float RdaAdaptationData::ant_manual_setup_ielmin() const
{
   constexpr float SCALE = 360.0f / 65536.0f;
   return p->antManualSetup_.ielmin_ * SCALE;
}

float RdaAdaptationData::ant_manual_setup_ielmax() const
{
   constexpr float SCALE = 360.0f / 65536.0f;
   return p->antManualSetup_.ielmax_ * SCALE;
}

uint32_t RdaAdaptationData::ant_manual_setup_fazvelmax() const
{
   return p->antManualSetup_.fazvelmax_;
}

uint32_t RdaAdaptationData::ant_manual_setup_felvelmax() const
{
   return p->antManualSetup_.felvelmax_;
}

int32_t RdaAdaptationData::ant_manual_setup_ignd_hgt() const
{
   return p->antManualSetup_.igndHgt_;
}

uint32_t RdaAdaptationData::ant_manual_setup_irad_hgt() const
{
   return p->antManualSetup_.iradHgt_;
}

float RdaAdaptationData::az_pos_sustain_drive() const
{
   return p->azPosSustainDrive_;
}

float RdaAdaptationData::az_neg_sustain_drive() const
{
   return p->azNegSustainDrive_;
}

float RdaAdaptationData::az_nom_pos_drive_slope() const
{
   return p->azNomPosDriveSlope_;
}

float RdaAdaptationData::az_nom_neg_drive_slope() const
{
   return p->azNomNegDriveSlope_;
}

float RdaAdaptationData::az_feedback_slope() const
{
   return p->azFeedbackSlope_;
}

float RdaAdaptationData::el_pos_sustain_drive() const
{
   return p->elPosSustainDrive_;
}

float RdaAdaptationData::el_neg_sustain_drive() const
{
   return p->elNegSustainDrive_;
}

float RdaAdaptationData::el_nom_pos_drive_slope() const
{
   return p->elNomPosDriveSlope_;
}

float RdaAdaptationData::el_nom_neg_drive_slope() const
{
   return p->elNomNegDriveSlope_;
}

float RdaAdaptationData::el_feedback_slope() const
{
   return p->elFeedbackSlope_;
}

float RdaAdaptationData::el_first_slope() const
{
   return p->elFirstSlope_;
}

float RdaAdaptationData::el_second_slope() const
{
   return p->elSecondSlope_;
}

float RdaAdaptationData::el_third_slope() const
{
   return p->elThirdSlope_;
}

float RdaAdaptationData::el_droop_pos() const
{
   return p->elDroopPos_;
}

float RdaAdaptationData::el_off_neutral_drive() const
{
   return p->elOffNeutralDrive_;
}

float RdaAdaptationData::az_intertia() const
{
   return p->azIntertia_;
}

float RdaAdaptationData::el_inertia() const
{
   return p->elInertia_;
}

uint32_t RdaAdaptationData::rvp8nv_iwaveguide_length() const
{
   return p->rvp8nvIwaveguideLength_;
}

float RdaAdaptationData::v_rnscale(unsigned i) const
{
   return p->vRnscale_[i];
}

float RdaAdaptationData::vel_data_tover() const
{
   return p->velDataTover_;
}

float RdaAdaptationData::width_data_tover() const
{
   return p->widthDataTover_;
}

float RdaAdaptationData::doppler_range_start() const
{
   return p->dopplerRangeStart_;
}

uint32_t RdaAdaptationData::max_el_index() const
{
   return p->maxElIndex_;
}

float RdaAdaptationData::seg2_lim() const
{
   return p->seg2Lim_;
}

float RdaAdaptationData::seg3_lim() const
{
   return p->seg3Lim_;
}

float RdaAdaptationData::seg4_lim() const
{
   return p->seg4Lim_;
}

uint32_t RdaAdaptationData::nbr_el_segments() const
{
   return p->nbrElSegments_;
}

float RdaAdaptationData::h_noise_long() const
{
   return p->hNoiseLong_;
}

float RdaAdaptationData::ant_noise_temp() const
{
   return p->antNoiseTemp_;
}

float RdaAdaptationData::h_noise_short() const
{
   return p->hNoiseShort_;
}

float RdaAdaptationData::h_noise_tolerance() const
{
   return p->hNoiseTolerance_;
}

float RdaAdaptationData::min_h_dyn_range() const
{
   return p->minHDynRange_;
}

bool RdaAdaptationData::gen_installed() const
{
   return p->genInstalled_;
}

bool RdaAdaptationData::gen_exercise() const
{
   return p->genExercise_;
}

float RdaAdaptationData::v_noise_tolerance() const
{
   return p->vNoiseTolerance_;
}

float RdaAdaptationData::min_v_dyn_range() const
{
   return p->minVDynRange_;
}

float RdaAdaptationData::zdr_bias_dgrad_lim() const
{
   return p->zdrBiasDgradLim_;
}

float RdaAdaptationData::baseline_zdr_bias() const
{
   return p->baselineZdrBias_;
}

float RdaAdaptationData::v_noise_long() const
{
   return p->vNoiseLong_;
}

float RdaAdaptationData::v_noise_short() const
{
   return p->vNoiseShort_;
}

float RdaAdaptationData::zdr_data_tover() const
{
   return p->zdrDataTover_;
}

float RdaAdaptationData::phi_data_tover() const
{
   return p->phiDataTover_;
}

float RdaAdaptationData::rho_data_tover() const
{
   return p->rhoDataTover_;
}

float RdaAdaptationData::stalo_power_dgrad_limit() const
{
   return p->staloPowerDgradLimit_;
}

float RdaAdaptationData::stalo_power_maint_limit() const
{
   return p->staloPowerMaintLimit_;
}

float RdaAdaptationData::min_h_pwr_sense() const
{
   return p->minHPwrSense_;
}

float RdaAdaptationData::min_v_pwr_sense() const
{
   return p->minVPwrSense_;
}

float RdaAdaptationData::h_pwr_sense_offset() const
{
   return p->hPwrSenseOffset_;
}

float RdaAdaptationData::v_pwr_sense_offset() const
{
   return p->vPwrSenseOffset_;
}

float RdaAdaptationData::ps_gain_ref() const
{
   return p->psGainRef_;
}

float RdaAdaptationData::rf_pallet_broad_loss() const
{
   return p->rfPalletBroadLoss_;
}

float RdaAdaptationData::ame_ps_tolerance() const
{
   return p->amePsTolerance_;
}

float RdaAdaptationData::ame_max_temp() const
{
   return p->ameMaxTemp_;
}

float RdaAdaptationData::ame_min_temp() const
{
   return p->ameMinTemp_;
}

float RdaAdaptationData::rcvr_mod_max_temp() const
{
   return p->rcvrModMaxTemp_;
}

float RdaAdaptationData::rcvr_mod_min_temp() const
{
   return p->rcvrModMinTemp_;
}

float RdaAdaptationData::bite_mod_max_temp() const
{
   return p->biteModMaxTemp_;
}

float RdaAdaptationData::bite_mod_min_temp() const
{
   return p->biteModMinTemp_;
}

uint32_t RdaAdaptationData::default_polarization() const
{
   return p->defaultPolarization_;
}

float RdaAdaptationData::tr_limit_dgrad_limit() const
{
   return p->trLimitDgradLimit_;
}

float RdaAdaptationData::tr_limit_fail_limit() const
{
   return p->trLimitFailLimit_;
}

bool RdaAdaptationData::rfp_stepper_enabled() const
{
   return p->rfpStepperEnabled_;
}

float RdaAdaptationData::ame_current_tolerance() const
{
   return p->ameCurrentTolerance_;
}

uint32_t RdaAdaptationData::h_only_polarization() const
{
   return p->hOnlyPolarization_;
}

uint32_t RdaAdaptationData::v_only_polarization() const
{
   return p->vOnlyPolarization_;
}

float RdaAdaptationData::sun_bias() const
{
   return p->sunBias_;
}

float RdaAdaptationData::a_min_shelter_temp_warn() const
{
   return p->aMinShelterTempWarn_;
}

float RdaAdaptationData::power_meter_zero() const
{
   return p->powerMeterZero_;
}

float RdaAdaptationData::txb_baseline() const
{
   return p->txbBaseline_;
}

float RdaAdaptationData::txb_alarm_thresh() const
{
   return p->txbAlarmThresh_;
}

bool RdaAdaptationData::Parse(std::istream& is)
{
   BOOST_LOG_TRIVIAL(trace)
      << logPrefix_ << "Parsing RDA Adaptation Data (Message Type 18)";

   bool   messageValid = true;
   size_t bytesRead    = 0;

   p->adapFileName_.resize(12);
   p->adapFormat_.resize(4);
   p->adapRevision_.resize(4);
   p->adapDate_.resize(12);
   p->adapTime_.resize(12);
   p->siteName_.resize(4);

   is.read(&p->adapFileName_[0], 12); // 0-11
   is.read(&p->adapFormat_[0], 4);    // 12-15
   is.read(&p->adapRevision_[0], 4);  // 16-19
   is.read(&p->adapDate_[0], 12);     // 20-31
   is.read(&p->adapTime_[0], 12);     // 32-43

   is.read(reinterpret_cast<char*>(&p->lowerPreLimit_), 4); // 44-47
   is.read(reinterpret_cast<char*>(&p->azLat_), 4);         // 48-51
   is.read(reinterpret_cast<char*>(&p->upperPreLimit_), 4); // 52-55
   is.read(reinterpret_cast<char*>(&p->elLat_), 4);         // 56-59
   is.read(reinterpret_cast<char*>(&p->parkaz_), 4);        // 60-63
   is.read(reinterpret_cast<char*>(&p->parkel_), 4);        // 64-67

   is.read(reinterpret_cast<char*>(&p->aFuelConv_[0]),
           p->aFuelConv_.size() * 4); // 68-111

   is.read(reinterpret_cast<char*>(&p->aMinShelterTemp_), 4);       // 112-115
   is.read(reinterpret_cast<char*>(&p->aMaxShelterTemp_), 4);       // 116-119
   is.read(reinterpret_cast<char*>(&p->aMinShelterAcTempDiff_), 4); // 120-123
   is.read(reinterpret_cast<char*>(&p->aMaxXmtrAirTemp_), 4);       // 124-127
   is.read(reinterpret_cast<char*>(&p->aMaxRadTemp_), 4);           // 128-131
   is.read(reinterpret_cast<char*>(&p->aMaxRadTempRise_), 4);       // 132-135
   is.read(reinterpret_cast<char*>(&p->lowerDeadLimit_), 4);        // 136-139
   is.read(reinterpret_cast<char*>(&p->upperDeadLimit_), 4);        // 140-143

   is.seekg(4, std::ios_base::cur); // 144-147

   is.read(reinterpret_cast<char*>(&p->aMinGenRoomTemp_), 4); // 148-151
   is.read(reinterpret_cast<char*>(&p->aMaxGenRoomTemp_), 4); // 152-155
   is.read(reinterpret_cast<char*>(&p->spip5VRegLim_), 4);    // 156-159
   is.read(reinterpret_cast<char*>(&p->spip15VRegLim_), 4);   // 160-163

   is.seekg(12, std::ios_base::cur); // 164-175

   ReadBoolean(is, p->rpgCoLocated_);        // 176-179
   ReadBoolean(is, p->specFilterInstalled_); // 180-183
   ReadBoolean(is, p->tpsInstalled_);        // 184-187
   ReadBoolean(is, p->rmsInstalled_);        // 188-191

   is.read(reinterpret_cast<char*>(&p->aHvdlTstInt_), 4);           // 192-195
   is.read(reinterpret_cast<char*>(&p->aRpgLtInt_), 4);             // 196-199
   is.read(reinterpret_cast<char*>(&p->aMinStabUtilPwrTime_), 4);   // 200-203
   is.read(reinterpret_cast<char*>(&p->aGenAutoExerInterval_), 4);  // 204-207
   is.read(reinterpret_cast<char*>(&p->aUtilPwrSwReqInterval_), 4); // 208-211
   is.read(reinterpret_cast<char*>(&p->aLowFuelLevel_), 4);         // 212-215
   is.read(reinterpret_cast<char*>(&p->configChanNumber_), 4);      // 216-219

   is.seekg(4, std::ios_base::cur); // 220-223

   is.read(reinterpret_cast<char*>(&p->redundantChanConfig_), 4); // 224-227

   is.read(reinterpret_cast<char*>(&p->attenTable_[0]),
           p->attenTable_.size() * 4); // 228-643

   is.seekg(24, std::ios_base::cur);                         // 644-667
   is.read(reinterpret_cast<char*>(&p->pathLosses_[7]), 4);  // 668-671
   is.seekg(20, std::ios_base::cur);                         // 672-691
   is.read(reinterpret_cast<char*>(&p->pathLosses_[13]), 4); // 692-695
   is.seekg(56, std::ios_base::cur);                         // 696-751
   is.read(reinterpret_cast<char*>(&p->pathLosses_[28]), 4); // 752-755
   is.read(reinterpret_cast<char*>(&p->pathLosses_[29]), 4); // 756-759
   is.seekg(8, std::ios_base::cur);                          // 760-767
   is.read(reinterpret_cast<char*>(&p->pathLosses_[32]), 4); // 768-771
   is.read(reinterpret_cast<char*>(&p->pathLosses_[33]), 4); // 772-775
   is.seekg(4, std::ios_base::cur);                          // 776-779
   is.read(reinterpret_cast<char*>(&p->pathLosses_[35]), 4); // 780-783
   is.seekg(12, std::ios_base::cur);                         // 784-795
   is.read(reinterpret_cast<char*>(&p->pathLosses_[39]), 4); // 796-799
   is.read(reinterpret_cast<char*>(&p->pathLosses_[40]), 4); // 800-803
   is.seekg(4, std::ios_base::cur);                          // 804-807
   is.read(reinterpret_cast<char*>(&p->pathLosses_[42]), 4); // 808-811
   is.read(reinterpret_cast<char*>(&p->pathLosses_[43]), 4); // 812-815
   is.read(reinterpret_cast<char*>(&p->pathLosses_[44]), 4); // 816-819
   is.read(reinterpret_cast<char*>(&p->pathLosses_[45]), 4); // 820-823
   is.read(reinterpret_cast<char*>(&p->pathLosses_[46]), 4); // 824-827
   is.read(reinterpret_cast<char*>(&p->pathLosses_[47]), 4); // 828-831
   is.read(reinterpret_cast<char*>(&p->pathLosses_[48]), 4); // 832-835
   is.read(reinterpret_cast<char*>(&p->pathLosses_[49]), 4); // 836-839
   is.seekg(4, std::ios_base::cur);                          // 840-843
   is.read(reinterpret_cast<char*>(&p->pathLosses_[51]), 4); // 844-847
   is.read(reinterpret_cast<char*>(&p->pathLosses_[52]), 4); // 848-851
   is.read(reinterpret_cast<char*>(&p->pathLosses_[53]), 4); // 852-855
   is.seekg(8, std::ios_base::cur);                          // 856-863
   is.read(reinterpret_cast<char*>(&p->pathLosses_[56]), 4); // 864-867
   is.read(reinterpret_cast<char*>(&p->pathLosses_[57]), 4); // 868-871
   is.read(reinterpret_cast<char*>(&p->pathLosses_[58]), 4); // 872-875
   is.read(reinterpret_cast<char*>(&p->pathLosses_[59]), 4); // 876-879
   is.read(reinterpret_cast<char*>(&p->pathLosses_[60]), 4); // 880-883
   is.read(reinterpret_cast<char*>(&p->pathLosses_[61]), 4); // 884-887
   is.seekg(4, std::ios_base::cur);                          // 888-891
   is.read(reinterpret_cast<char*>(&p->pathLosses_[63]), 4); // 892-895
   is.read(reinterpret_cast<char*>(&p->pathLosses_[64]), 4); // 896-899
   is.read(reinterpret_cast<char*>(&p->pathLosses_[65]), 4); // 900-903
   is.read(reinterpret_cast<char*>(&p->pathLosses_[66]), 4); // 904-907
   is.read(reinterpret_cast<char*>(&p->pathLosses_[67]), 4); // 908-911
   is.read(reinterpret_cast<char*>(&p->pathLosses_[68]), 4); // 912-915
   is.seekg(4, std::ios_base::cur);                          // 916-919
   is.read(reinterpret_cast<char*>(&p->pathLosses_[70]), 4); // 920-923
   is.read(reinterpret_cast<char*>(&p->pathLosses_[71]), 4); // 924-927

   is.seekg(8, std::ios_base::cur); // 928-935

   is.read(reinterpret_cast<char*>(&p->vTsCw_), 4); // 936-939

   is.read(reinterpret_cast<char*>(&p->hRnscale_[0]),
           p->hRnscale_.size() * 4); // 940-991

   is.read(reinterpret_cast<char*>(&p->atmos_[0]),
           p->atmos_.size() * 4); // 992-1043

   is.read(reinterpret_cast<char*>(&p->elIndex_[0]),
           p->elIndex_.size() * 4); // 1044-1091

   is.read(reinterpret_cast<char*>(&p->tfreqMhz_), 4);      // 1092-1095
   is.read(reinterpret_cast<char*>(&p->baseDataTcn_), 4);   // 1096-1099
   is.read(reinterpret_cast<char*>(&p->reflDataTover_), 4); // 1100-1103
   is.read(reinterpret_cast<char*>(&p->tarHDbz0Lp_), 4);    // 1104-1107
   is.read(reinterpret_cast<char*>(&p->tarVDbz0Lp_), 4);    // 1108-1111
   is.read(reinterpret_cast<char*>(&p->initPhiDp_), 4);     // 1112-1115
   is.read(reinterpret_cast<char*>(&p->normInitPhiDp_), 4); // 1116-1119
   is.read(reinterpret_cast<char*>(&p->lxLp_), 4);          // 1120-1123
   is.read(reinterpret_cast<char*>(&p->lxSp_), 4);          // 1124-1127
   is.read(reinterpret_cast<char*>(&p->meteorParam_), 4);   // 1128-1131

   is.seekg(4, std::ios_base::cur); // 1132-1135

   is.read(reinterpret_cast<char*>(&p->antennaGain_), 4); // 1136-1139

   is.seekg(12, std::ios_base::cur); // 1140-1151

   is.read(reinterpret_cast<char*>(&p->velDegradLimit_), 4);       // 1152-1155
   is.read(reinterpret_cast<char*>(&p->wthDegradLimit_), 4);       // 1156-1159
   is.read(reinterpret_cast<char*>(&p->hNoisetempDgradLimit_), 4); // 1160-1163
   is.read(reinterpret_cast<char*>(&p->hMinNoisetemp_), 4);        // 1164-1167
   is.read(reinterpret_cast<char*>(&p->vNoisetempDgradLimit_), 4); // 1168-1171
   is.read(reinterpret_cast<char*>(&p->vMinNoisetemp_), 4);        // 1172-1175
   is.read(reinterpret_cast<char*>(&p->klyDegradeLimit_), 4);      // 1176-1179
   is.read(reinterpret_cast<char*>(&p->tsCoho_), 4);               // 1180-1183
   is.read(reinterpret_cast<char*>(&p->hTsCw_), 4);                // 1184-1187

   is.seekg(8, std::ios_base::cur); // 1188-1195

   is.read(reinterpret_cast<char*>(&p->tsStalo_), 4);              // 1196-1199
   is.read(reinterpret_cast<char*>(&p->ameHNoiseEnr_), 4);         // 1200-1203
   is.read(reinterpret_cast<char*>(&p->xmtrPeakPwrHighLimit_), 4); // 1204-1207
   is.read(reinterpret_cast<char*>(&p->xmtrPeakPwrLowLimit_), 4);  // 1208-1211
   is.read(reinterpret_cast<char*>(&p->hDbz0DeltaLimit_), 4);      // 1212-1215
   is.read(reinterpret_cast<char*>(&p->threshold1_), 4);           // 1216-1219
   is.read(reinterpret_cast<char*>(&p->threshold2_), 4);           // 1220-1223
   is.read(reinterpret_cast<char*>(&p->clutSuppDgradLim_), 4);     // 1224-1227

   is.seekg(4, std::ios_base::cur); // 1228-1231

   is.read(reinterpret_cast<char*>(&p->range0Value_), 4);     // 1232-1235
   is.read(reinterpret_cast<char*>(&p->xmtrPwrMtrScale_), 4); // 1236-1239
   is.read(reinterpret_cast<char*>(&p->vDbz0DeltaLimit_), 4); // 1240-1243
   is.read(reinterpret_cast<char*>(&p->tarHDbz0Sp_), 4);      // 1244-1247
   is.read(reinterpret_cast<char*>(&p->tarVDbz0Sp_), 4);      // 1248-1251
   is.read(reinterpret_cast<char*>(&p->deltaprf_), 4);        // 1252-1255

   is.seekg(8, std::ios_base::cur); // 1256-1263

   is.read(reinterpret_cast<char*>(&p->tauSp_), 4);       // 1264-1267
   is.read(reinterpret_cast<char*>(&p->tauLp_), 4);       // 1268-1271
   is.read(reinterpret_cast<char*>(&p->ncDeadValue_), 4); // 1272-1275
   is.read(reinterpret_cast<char*>(&p->tauRfSp_), 4);     // 1276-1279
   is.read(reinterpret_cast<char*>(&p->tauRfLp_), 4);     // 1280-1283
   is.read(reinterpret_cast<char*>(&p->seg1Lim_), 4);     // 1284-1287
   is.read(reinterpret_cast<char*>(&p->slatsec_), 4);     // 1288-1291
   is.read(reinterpret_cast<char*>(&p->slonsec_), 4);     // 1292-1295

   is.seekg(4, std::ios_base::cur); // 1296-1299

   is.read(reinterpret_cast<char*>(&p->slatdeg_), 4); // 1300-1303
   is.read(reinterpret_cast<char*>(&p->slatmin_), 4); // 1304-1307
   is.read(reinterpret_cast<char*>(&p->slondeg_), 4); // 1308-1311
   is.read(reinterpret_cast<char*>(&p->slonmin_), 4); // 1312-1315
   ReadChar(is, p->slatdir_);                         // 1316-1319
   ReadChar(is, p->slondir_);                         // 1320-1323

   is.seekg(7036, std::ios_base::cur); // 1324-8359

   is.read(reinterpret_cast<char*>(&p->azCorrectionFactor_), 4); // 8360-8363
   is.read(reinterpret_cast<char*>(&p->elCorrectionFactor_), 4); // 8364-8367
   is.read(&p->siteName_[0], 4);                                 // 8368-8371
   is.read(reinterpret_cast<char*>(&p->antManualSetup_.ielmin_),
           4); // 8372-8375
   is.read(reinterpret_cast<char*>(&p->antManualSetup_.ielmax_),
           4); // 8376-8379
   is.read(reinterpret_cast<char*>(&p->antManualSetup_.fazvelmax_),
           4); // 8380-8383
   is.read(reinterpret_cast<char*>(&p->antManualSetup_.felvelmax_),
           4); // 8384-8387
   is.read(reinterpret_cast<char*>(&p->antManualSetup_.igndHgt_),
           4); // 8388-8391
   is.read(reinterpret_cast<char*>(&p->antManualSetup_.iradHgt_),
           4);                                                   // 8392-8395
   is.read(reinterpret_cast<char*>(&p->azPosSustainDrive_), 4);  // 8396-8399
   is.read(reinterpret_cast<char*>(&p->azNegSustainDrive_), 4);  // 8400-8403
   is.read(reinterpret_cast<char*>(&p->azNomPosDriveSlope_), 4); // 8404-8407
   is.read(reinterpret_cast<char*>(&p->azNomNegDriveSlope_), 4); // 8408-8411
   is.read(reinterpret_cast<char*>(&p->azFeedbackSlope_), 4);    // 8412-8415
   is.read(reinterpret_cast<char*>(&p->elPosSustainDrive_), 4);  // 8416-8419
   is.read(reinterpret_cast<char*>(&p->elNegSustainDrive_), 4);  // 8420-8423
   is.read(reinterpret_cast<char*>(&p->elNomPosDriveSlope_), 4); // 8424-8427
   is.read(reinterpret_cast<char*>(&p->elNomNegDriveSlope_), 4); // 8428-8431
   is.read(reinterpret_cast<char*>(&p->elFeedbackSlope_), 4);    // 8432-8435
   is.read(reinterpret_cast<char*>(&p->elFirstSlope_), 4);       // 8436-8439
   is.read(reinterpret_cast<char*>(&p->elSecondSlope_), 4);      // 8440-8443
   is.read(reinterpret_cast<char*>(&p->elThirdSlope_), 4);       // 8444-8447
   is.read(reinterpret_cast<char*>(&p->elDroopPos_), 4);         // 8448-8451
   is.read(reinterpret_cast<char*>(&p->elOffNeutralDrive_), 4);  // 8452-8455
   is.read(reinterpret_cast<char*>(&p->azIntertia_), 4);         // 8456-8459
   is.read(reinterpret_cast<char*>(&p->elInertia_), 4);          // 8460-8463

   is.seekg(232, std::ios_base::cur); // 8464-8695

   is.read(reinterpret_cast<char*>(&p->rvp8nvIwaveguideLength_),
           4); // 8696-8699

   is.read(reinterpret_cast<char*>(&p->vRnscale_[0]),
           11 * 4); // 8700-8743

   is.read(reinterpret_cast<char*>(&p->velDataTover_), 4);     // 8744-8747
   is.read(reinterpret_cast<char*>(&p->widthDataTover_), 4);   // 8748-8751
   is.read(reinterpret_cast<char*>(&p->vRnscale_[11]), 2 * 4); // 8752-8759

   is.seekg(4, std::ios_base::cur); // 8760-8763

   is.read(reinterpret_cast<char*>(&p->dopplerRangeStart_), 4); // 8764-8767
   is.read(reinterpret_cast<char*>(&p->maxElIndex_), 4);        // 8768-8771
   is.read(reinterpret_cast<char*>(&p->seg2Lim_), 4);           // 8772-8775
   is.read(reinterpret_cast<char*>(&p->seg3Lim_), 4);           // 8776-8779
   is.read(reinterpret_cast<char*>(&p->seg4Lim_), 4);           // 8780-8783
   is.read(reinterpret_cast<char*>(&p->nbrElSegments_), 4);     // 8784-8787
   is.read(reinterpret_cast<char*>(&p->hNoiseLong_), 4);        // 8788-8791
   is.read(reinterpret_cast<char*>(&p->antNoiseTemp_), 4);      // 8792-8795
   is.read(reinterpret_cast<char*>(&p->hNoiseShort_), 4);       // 8796-8799
   is.read(reinterpret_cast<char*>(&p->hNoiseTolerance_), 4);   // 8800-8803
   is.read(reinterpret_cast<char*>(&p->minHDynRange_), 4);      // 8804-8807
   ReadBoolean(is, p->genInstalled_);                           // 8808-8811
   ReadBoolean(is, p->genExercise_);                            // 8812-8815
   is.read(reinterpret_cast<char*>(&p->vNoiseTolerance_), 4);   // 8816-8819
   is.read(reinterpret_cast<char*>(&p->minVDynRange_), 4);      // 8820-8823
   is.read(reinterpret_cast<char*>(&p->zdrBiasDgradLim_), 4);   // 8824-8827
   is.read(reinterpret_cast<char*>(&p->baselineZdrBias_), 4);   // 8828-8831

   is.seekg(12, std::ios_base::cur); // 8832-8843

   is.read(reinterpret_cast<char*>(&p->vNoiseLong_), 4);           // 8844-8847
   is.read(reinterpret_cast<char*>(&p->vNoiseShort_), 4);          // 8848-8851
   is.read(reinterpret_cast<char*>(&p->zdrDataTover_), 4);         // 8852-8855
   is.read(reinterpret_cast<char*>(&p->phiDataTover_), 4);         // 8856-8859
   is.read(reinterpret_cast<char*>(&p->rhoDataTover_), 4);         // 8860-8863
   is.read(reinterpret_cast<char*>(&p->staloPowerDgradLimit_), 4); // 8864-8867
   is.read(reinterpret_cast<char*>(&p->staloPowerMaintLimit_), 4); // 8868-8871
   is.read(reinterpret_cast<char*>(&p->minHPwrSense_), 4);         // 8872-8875
   is.read(reinterpret_cast<char*>(&p->minVPwrSense_), 4);         // 8876-8879
   is.read(reinterpret_cast<char*>(&p->hPwrSenseOffset_), 4);      // 8880-8883
   is.read(reinterpret_cast<char*>(&p->vPwrSenseOffset_), 4);      // 8884-8887
   is.read(reinterpret_cast<char*>(&p->psGainRef_), 4);            // 8888-8891
   is.read(reinterpret_cast<char*>(&p->rfPalletBroadLoss_), 4);    // 8892-8895

   is.seekg(64, std::ios_base::cur); // 8896-8959

   is.read(reinterpret_cast<char*>(&p->amePsTolerance_), 4);      // 8960-8963
   is.read(reinterpret_cast<char*>(&p->ameMaxTemp_), 4);          // 8964-8967
   is.read(reinterpret_cast<char*>(&p->ameMinTemp_), 4);          // 8968-8971
   is.read(reinterpret_cast<char*>(&p->rcvrModMaxTemp_), 4);      // 8972-8975
   is.read(reinterpret_cast<char*>(&p->rcvrModMinTemp_), 4);      // 8976-8979
   is.read(reinterpret_cast<char*>(&p->biteModMaxTemp_), 4);      // 8980-8983
   is.read(reinterpret_cast<char*>(&p->biteModMinTemp_), 4);      // 8984-8987
   is.read(reinterpret_cast<char*>(&p->defaultPolarization_), 4); // 8988-8991
   is.read(reinterpret_cast<char*>(&p->trLimitDgradLimit_), 4);   // 8992-8995
   is.read(reinterpret_cast<char*>(&p->trLimitFailLimit_), 4);    // 8996-8999
   ReadBoolean(is, p->rfpStepperEnabled_);                        // 9000-9003

   is.seekg(4, std::ios_base::cur); // 9004-9007

   is.read(reinterpret_cast<char*>(&p->ameCurrentTolerance_), 4); // 9008-9011
   is.read(reinterpret_cast<char*>(&p->hOnlyPolarization_), 4);   // 9012-9015
   is.read(reinterpret_cast<char*>(&p->vOnlyPolarization_), 4);   // 9016-9019

   is.seekg(8, std::ios_base::cur); // 9020-9027

   is.read(reinterpret_cast<char*>(&p->sunBias_), 4);             // 9028-9031
   is.read(reinterpret_cast<char*>(&p->aMinShelterTempWarn_), 4); // 9032-9035
   is.read(reinterpret_cast<char*>(&p->powerMeterZero_), 4);      // 9036-9039
   is.read(reinterpret_cast<char*>(&p->txbBaseline_), 4);         // 9040-9043
   is.read(reinterpret_cast<char*>(&p->txbAlarmThresh_), 4);      // 9044-9047

   is.seekg(420, std::ios_base::cur); // 9048-9467

   bytesRead += 9468;

   p->lowerPreLimit_ = SwapFloat(p->lowerPreLimit_);
   p->azLat_         = SwapFloat(p->azLat_);
   p->upperPreLimit_ = SwapFloat(p->upperPreLimit_);
   p->elLat_         = SwapFloat(p->elLat_);
   p->parkaz_        = SwapFloat(p->parkaz_);
   p->parkel_        = SwapFloat(p->parkel_);

   SwapArray(p->aFuelConv_);

   p->aMinShelterTemp_       = SwapFloat(p->aMinShelterTemp_);
   p->aMaxShelterTemp_       = SwapFloat(p->aMaxShelterTemp_);
   p->aMinShelterAcTempDiff_ = SwapFloat(p->aMinShelterAcTempDiff_);
   p->aMaxXmtrAirTemp_       = SwapFloat(p->aMaxXmtrAirTemp_);
   p->aMaxRadTemp_           = SwapFloat(p->aMaxRadTemp_);
   p->aMaxRadTempRise_       = SwapFloat(p->aMaxRadTempRise_);
   p->lowerDeadLimit_        = SwapFloat(p->lowerDeadLimit_);
   p->upperDeadLimit_        = SwapFloat(p->upperDeadLimit_);
   p->aMinGenRoomTemp_       = SwapFloat(p->aMinGenRoomTemp_);
   p->aMaxGenRoomTemp_       = SwapFloat(p->aMaxGenRoomTemp_);
   p->spip5VRegLim_          = SwapFloat(p->spip5VRegLim_);
   p->spip15VRegLim_         = SwapFloat(p->spip15VRegLim_);
   p->aHvdlTstInt_           = ntohl(p->aHvdlTstInt_);
   p->aRpgLtInt_             = ntohl(p->aRpgLtInt_);
   p->aMinStabUtilPwrTime_   = ntohl(p->aMinStabUtilPwrTime_);
   p->aGenAutoExerInterval_  = ntohl(p->aGenAutoExerInterval_);
   p->aUtilPwrSwReqInterval_ = ntohl(p->aUtilPwrSwReqInterval_);
   p->aLowFuelLevel_         = SwapFloat(p->aLowFuelLevel_);
   p->configChanNumber_      = ntohl(p->configChanNumber_);
   p->redundantChanConfig_   = ntohl(p->redundantChanConfig_);

   SwapArray(p->attenTable_);
   SwapMap(p->pathLosses_);

   p->vTsCw_ = SwapFloat(p->vTsCw_);

   SwapArray(p->hRnscale_);
   SwapArray(p->atmos_);
   SwapArray(p->elIndex_);

   p->tfreqMhz_                  = ntohl(p->tfreqMhz_);
   p->baseDataTcn_               = SwapFloat(p->baseDataTcn_);
   p->reflDataTover_             = SwapFloat(p->reflDataTover_);
   p->tarHDbz0Lp_                = SwapFloat(p->tarHDbz0Lp_);
   p->tarVDbz0Lp_                = SwapFloat(p->tarVDbz0Lp_);
   p->initPhiDp_                 = ntohl(p->initPhiDp_);
   p->normInitPhiDp_             = ntohl(p->normInitPhiDp_);
   p->lxLp_                      = SwapFloat(p->lxLp_);
   p->lxSp_                      = SwapFloat(p->lxSp_);
   p->meteorParam_               = SwapFloat(p->meteorParam_);
   p->antennaGain_               = SwapFloat(p->antennaGain_);
   p->velDegradLimit_            = SwapFloat(p->velDegradLimit_);
   p->wthDegradLimit_            = SwapFloat(p->wthDegradLimit_);
   p->hNoisetempDgradLimit_      = SwapFloat(p->hNoisetempDgradLimit_);
   p->hMinNoisetemp_             = ntohl(p->hMinNoisetemp_);
   p->vNoisetempDgradLimit_      = SwapFloat(p->vNoisetempDgradLimit_);
   p->vMinNoisetemp_             = ntohl(p->vMinNoisetemp_);
   p->klyDegradeLimit_           = SwapFloat(p->klyDegradeLimit_);
   p->tsCoho_                    = SwapFloat(p->tsCoho_);
   p->hTsCw_                     = SwapFloat(p->hTsCw_);
   p->tsStalo_                   = SwapFloat(p->tsStalo_);
   p->ameHNoiseEnr_              = SwapFloat(p->ameHNoiseEnr_);
   p->xmtrPeakPwrHighLimit_      = SwapFloat(p->xmtrPeakPwrHighLimit_);
   p->xmtrPeakPwrLowLimit_       = SwapFloat(p->xmtrPeakPwrLowLimit_);
   p->hDbz0DeltaLimit_           = SwapFloat(p->hDbz0DeltaLimit_);
   p->threshold1_                = SwapFloat(p->threshold1_);
   p->threshold2_                = SwapFloat(p->threshold2_);
   p->clutSuppDgradLim_          = SwapFloat(p->clutSuppDgradLim_);
   p->range0Value_               = SwapFloat(p->range0Value_);
   p->xmtrPwrMtrScale_           = SwapFloat(p->xmtrPwrMtrScale_);
   p->vDbz0DeltaLimit_           = SwapFloat(p->vDbz0DeltaLimit_);
   p->tarHDbz0Sp_                = SwapFloat(p->tarHDbz0Sp_);
   p->tarVDbz0Sp_                = SwapFloat(p->tarVDbz0Sp_);
   p->deltaprf_                  = ntohl(p->deltaprf_);
   p->tauSp_                     = ntohl(p->tauSp_);
   p->tauLp_                     = ntohl(p->tauLp_);
   p->ncDeadValue_               = ntohl(p->ncDeadValue_);
   p->tauRfSp_                   = ntohl(p->tauRfSp_);
   p->tauRfLp_                   = ntohl(p->tauRfLp_);
   p->seg1Lim_                   = SwapFloat(p->seg1Lim_);
   p->slatsec_                   = SwapFloat(p->slatsec_);
   p->slonsec_                   = SwapFloat(p->slonsec_);
   p->slatdeg_                   = ntohl(p->slatdeg_);
   p->slatmin_                   = ntohl(p->slatmin_);
   p->slondeg_                   = ntohl(p->slondeg_);
   p->slonmin_                   = ntohl(p->slonmin_);
   p->azCorrectionFactor_        = SwapFloat(p->azCorrectionFactor_);
   p->elCorrectionFactor_        = SwapFloat(p->elCorrectionFactor_);
   p->antManualSetup_.ielmin_    = ntohl(p->antManualSetup_.ielmin_);
   p->antManualSetup_.ielmax_    = ntohl(p->antManualSetup_.ielmax_);
   p->antManualSetup_.fazvelmax_ = ntohl(p->antManualSetup_.fazvelmax_);
   p->antManualSetup_.felvelmax_ = ntohl(p->antManualSetup_.felvelmax_);
   p->antManualSetup_.igndHgt_   = ntohl(p->antManualSetup_.igndHgt_);
   p->antManualSetup_.iradHgt_   = ntohl(p->antManualSetup_.iradHgt_);
   p->azPosSustainDrive_         = SwapFloat(p->azPosSustainDrive_);
   p->azNegSustainDrive_         = SwapFloat(p->azNegSustainDrive_);
   p->azNomPosDriveSlope_        = SwapFloat(p->azNomPosDriveSlope_);
   p->azNomNegDriveSlope_        = SwapFloat(p->azNomNegDriveSlope_);
   p->azFeedbackSlope_           = SwapFloat(p->azFeedbackSlope_);
   p->elPosSustainDrive_         = SwapFloat(p->elPosSustainDrive_);
   p->elNegSustainDrive_         = SwapFloat(p->elNegSustainDrive_);
   p->elNomPosDriveSlope_        = SwapFloat(p->elNomPosDriveSlope_);
   p->elNomNegDriveSlope_        = SwapFloat(p->elNomNegDriveSlope_);
   p->elFeedbackSlope_           = SwapFloat(p->elFeedbackSlope_);
   p->elFirstSlope_              = SwapFloat(p->elFirstSlope_);
   p->elSecondSlope_             = SwapFloat(p->elSecondSlope_);
   p->elThirdSlope_              = SwapFloat(p->elThirdSlope_);
   p->elDroopPos_                = SwapFloat(p->elDroopPos_);
   p->elOffNeutralDrive_         = SwapFloat(p->elOffNeutralDrive_);
   p->azIntertia_                = SwapFloat(p->azIntertia_);
   p->elInertia_                 = SwapFloat(p->elInertia_);
   p->rvp8nvIwaveguideLength_    = ntohl(p->rvp8nvIwaveguideLength_);

   SwapArray(p->vRnscale_);

   p->velDataTover_         = SwapFloat(p->velDataTover_);
   p->widthDataTover_       = SwapFloat(p->widthDataTover_);
   p->dopplerRangeStart_    = SwapFloat(p->dopplerRangeStart_);
   p->maxElIndex_           = ntohl(p->maxElIndex_);
   p->seg2Lim_              = SwapFloat(p->seg2Lim_);
   p->seg3Lim_              = SwapFloat(p->seg3Lim_);
   p->seg4Lim_              = SwapFloat(p->seg4Lim_);
   p->nbrElSegments_        = ntohl(p->nbrElSegments_);
   p->hNoiseLong_           = SwapFloat(p->hNoiseLong_);
   p->antNoiseTemp_         = SwapFloat(p->antNoiseTemp_);
   p->hNoiseShort_          = SwapFloat(p->hNoiseShort_);
   p->hNoiseTolerance_      = SwapFloat(p->hNoiseTolerance_);
   p->minHDynRange_         = SwapFloat(p->minHDynRange_);
   p->vNoiseTolerance_      = SwapFloat(p->vNoiseTolerance_);
   p->minVDynRange_         = SwapFloat(p->minVDynRange_);
   p->zdrBiasDgradLim_      = SwapFloat(p->zdrBiasDgradLim_);
   p->baselineZdrBias_      = SwapFloat(p->baselineZdrBias_);
   p->vNoiseLong_           = SwapFloat(p->vNoiseLong_);
   p->vNoiseShort_          = SwapFloat(p->vNoiseShort_);
   p->zdrDataTover_         = SwapFloat(p->zdrDataTover_);
   p->phiDataTover_         = SwapFloat(p->phiDataTover_);
   p->rhoDataTover_         = SwapFloat(p->rhoDataTover_);
   p->staloPowerDgradLimit_ = SwapFloat(p->staloPowerDgradLimit_);
   p->staloPowerMaintLimit_ = SwapFloat(p->staloPowerMaintLimit_);
   p->minHPwrSense_         = SwapFloat(p->minHPwrSense_);
   p->minVPwrSense_         = SwapFloat(p->minVPwrSense_);
   p->hPwrSenseOffset_      = SwapFloat(p->hPwrSenseOffset_);
   p->vPwrSenseOffset_      = SwapFloat(p->vPwrSenseOffset_);
   p->psGainRef_            = SwapFloat(p->psGainRef_);
   p->rfPalletBroadLoss_    = SwapFloat(p->rfPalletBroadLoss_);
   p->amePsTolerance_       = SwapFloat(p->amePsTolerance_);
   p->ameMaxTemp_           = SwapFloat(p->ameMaxTemp_);
   p->ameMinTemp_           = SwapFloat(p->ameMinTemp_);
   p->rcvrModMaxTemp_       = SwapFloat(p->rcvrModMaxTemp_);
   p->rcvrModMinTemp_       = SwapFloat(p->rcvrModMinTemp_);
   p->biteModMaxTemp_       = SwapFloat(p->biteModMaxTemp_);
   p->biteModMinTemp_       = SwapFloat(p->biteModMinTemp_);
   p->defaultPolarization_  = ntohl(p->defaultPolarization_);
   p->trLimitDgradLimit_    = SwapFloat(p->trLimitDgradLimit_);
   p->trLimitFailLimit_     = SwapFloat(p->trLimitFailLimit_);
   p->ameCurrentTolerance_  = SwapFloat(p->ameCurrentTolerance_);
   p->hOnlyPolarization_    = ntohl(p->hOnlyPolarization_);
   p->vOnlyPolarization_    = ntohl(p->vOnlyPolarization_);
   p->sunBias_              = SwapFloat(p->sunBias_);
   p->aMinShelterTempWarn_  = SwapFloat(p->aMinShelterTempWarn_);
   p->powerMeterZero_       = SwapFloat(p->powerMeterZero_);
   p->txbBaseline_          = SwapFloat(p->txbBaseline_);
   p->txbAlarmThresh_       = SwapFloat(p->txbAlarmThresh_);

   if (!ValidateMessage(is, bytesRead))
   {
      messageValid = false;
   }

   return messageValid;
}

std::shared_ptr<RdaAdaptationData>
RdaAdaptationData::Create(Level2MessageHeader&& header, std::istream& is)
{
   std::shared_ptr<RdaAdaptationData> message =
      std::make_shared<RdaAdaptationData>();
   message->set_header(std::move(header));

   if (!message->Parse(is))
   {
      message.reset();
   }

   return message;
}

} // namespace rda
} // namespace wsr88d
} // namespace scwx
