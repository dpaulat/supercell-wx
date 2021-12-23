#include <scwx/wsr88d/rda/rda_status_data.hpp>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

static const std::string logPrefix_ = "[scwx::wsr88d::rda::rda_status_data] ";

class RdaStatusDataImpl
{
public:
   explicit RdaStatusDataImpl() :
       rdaStatus_ {0},
       operabilityStatus_ {0},
       controlStatus_ {0},
       auxiliaryPowerGeneratorState_ {0},
       averageTransmitterPower_ {0},
       horizontalReflectivityCalibrationCorrection_ {0},
       dataTransmissionEnabled_ {0},
       volumeCoveragePatternNumber_ {0},
       rdaControlAuthorization_ {0},
       rdaBuildNumber_ {0},
       operationalMode_ {0},
       superResolutionStatus_ {0},
       clutterMitigationDecisionStatus_ {0},
       avsetEbcRdaLogDataStatus_ {0},
       rdaAlarmSummary_ {0},
       commandAcknowledgement_ {0},
       channelControlStatus_ {0},
       spotBlankingStatus_ {0},
       bypassMapGenerationDate_ {0},
       bypassMapGenerationTime_ {0},
       clutterFilterMapGenerationDate_ {0},
       clutterFilterMapGenerationTime_ {0},
       verticalReflectivityCalibrationCorrection_ {0},
       transitionPowerSourceStatus_ {0},
       rmsControlStatus_ {0},
       performanceCheckStatus_ {0},
       alarmCodes_ {0},
       signalProcessingOptions_ {0},
       statusVersion_ {0} {};
   ~RdaStatusDataImpl() = default;

   uint16_t                 rdaStatus_;
   uint16_t                 operabilityStatus_;
   uint16_t                 controlStatus_;
   uint16_t                 auxiliaryPowerGeneratorState_;
   uint16_t                 averageTransmitterPower_;
   int16_t                  horizontalReflectivityCalibrationCorrection_;
   uint16_t                 dataTransmissionEnabled_;
   uint16_t                 volumeCoveragePatternNumber_;
   uint16_t                 rdaControlAuthorization_;
   uint16_t                 rdaBuildNumber_;
   uint16_t                 operationalMode_;
   uint16_t                 superResolutionStatus_;
   uint16_t                 clutterMitigationDecisionStatus_;
   uint16_t                 avsetEbcRdaLogDataStatus_;
   uint16_t                 rdaAlarmSummary_;
   uint16_t                 commandAcknowledgement_;
   uint16_t                 channelControlStatus_;
   uint16_t                 spotBlankingStatus_;
   uint16_t                 bypassMapGenerationDate_;
   uint16_t                 bypassMapGenerationTime_;
   uint16_t                 clutterFilterMapGenerationDate_;
   uint16_t                 clutterFilterMapGenerationTime_;
   int16_t                  verticalReflectivityCalibrationCorrection_;
   uint16_t                 transitionPowerSourceStatus_;
   uint16_t                 rmsControlStatus_;
   uint16_t                 performanceCheckStatus_;
   std::array<uint16_t, 14> alarmCodes_;
   uint16_t                 signalProcessingOptions_;
   uint16_t                 statusVersion_;
};

RdaStatusData::RdaStatusData() :
    Level2Message(), p(std::make_unique<RdaStatusDataImpl>())
{
}
RdaStatusData::~RdaStatusData() = default;

RdaStatusData::RdaStatusData(RdaStatusData&&) noexcept = default;
RdaStatusData& RdaStatusData::operator=(RdaStatusData&&) noexcept = default;

uint16_t RdaStatusData::rda_status() const
{
   return p->rdaStatus_;
}

uint16_t RdaStatusData::operability_status() const
{
   return p->operabilityStatus_;
}

uint16_t RdaStatusData::control_status() const
{
   return p->controlStatus_;
}

uint16_t RdaStatusData::auxiliary_power_generator_state() const
{
   return p->auxiliaryPowerGeneratorState_;
}

uint16_t RdaStatusData::average_transmitter_power() const
{
   return p->averageTransmitterPower_;
}

float RdaStatusData::horizontal_reflectivity_calibration_correction() const
{
   return p->horizontalReflectivityCalibrationCorrection_ * 0.01f;
}

uint16_t RdaStatusData::data_transmission_enabled() const
{
   return p->dataTransmissionEnabled_;
}

uint16_t RdaStatusData::volume_coverage_pattern_number() const
{
   return p->volumeCoveragePatternNumber_;
}

uint16_t RdaStatusData::rda_control_authorization() const
{
   return p->rdaControlAuthorization_;
}

uint16_t RdaStatusData::rda_build_number() const
{
   return p->rdaBuildNumber_;
}

uint16_t RdaStatusData::operational_mode() const
{
   return p->operationalMode_;
}

uint16_t RdaStatusData::super_resolution_status() const
{
   return p->superResolutionStatus_;
}

uint16_t RdaStatusData::clutter_mitigation_decision_status() const
{
   return p->clutterMitigationDecisionStatus_;
}

uint16_t RdaStatusData::avset_ebc_rda_log_data_status() const
{
   return p->avsetEbcRdaLogDataStatus_;
}

uint16_t RdaStatusData::rda_alarm_summary() const
{
   return p->rdaAlarmSummary_;
}

uint16_t RdaStatusData::command_acknowledgement() const
{
   return p->commandAcknowledgement_;
}

uint16_t RdaStatusData::channel_control_status() const
{
   return p->channelControlStatus_;
}

uint16_t RdaStatusData::spot_blanking_status() const
{
   return p->spotBlankingStatus_;
}

uint16_t RdaStatusData::bypass_map_generation_date() const
{
   return p->bypassMapGenerationDate_;
}

uint16_t RdaStatusData::bypass_map_generation_time() const
{
   return p->bypassMapGenerationTime_;
}

uint16_t RdaStatusData::clutter_filter_map_generation_date() const
{
   return p->clutterFilterMapGenerationDate_;
}

uint16_t RdaStatusData::clutter_filter_map_generation_time() const
{
   return p->clutterFilterMapGenerationTime_;
}

float RdaStatusData::vertical_reflectivity_calibration_correction() const
{
   return p->verticalReflectivityCalibrationCorrection_ * 0.01f;
}

uint16_t RdaStatusData::transition_power_source_status() const
{
   return p->transitionPowerSourceStatus_;
}

uint16_t RdaStatusData::rms_control_status() const
{
   return p->rmsControlStatus_;
}

uint16_t RdaStatusData::performance_check_status() const
{
   return p->performanceCheckStatus_;
}

uint16_t RdaStatusData::alarm_codes(unsigned i) const
{
   return p->alarmCodes_[i];
}

uint16_t RdaStatusData::signal_processing_options() const
{
   return p->signalProcessingOptions_;
}

uint16_t RdaStatusData::status_version() const
{
   return p->statusVersion_;
}

bool RdaStatusData::Parse(std::istream& is)
{
   BOOST_LOG_TRIVIAL(trace)
      << logPrefix_ << "Parsing RDA Status Data (Message Type 2)";

   bool   messageValid = true;
   size_t bytesRead    = 0;

   is.read(reinterpret_cast<char*>(&p->rdaStatus_), 2);                    // 1
   is.read(reinterpret_cast<char*>(&p->operabilityStatus_), 2);            // 2
   is.read(reinterpret_cast<char*>(&p->controlStatus_), 2);                // 3
   is.read(reinterpret_cast<char*>(&p->auxiliaryPowerGeneratorState_), 2); // 4
   is.read(reinterpret_cast<char*>(&p->averageTransmitterPower_), 2);      // 5
   is.read(
      reinterpret_cast<char*>(&p->horizontalReflectivityCalibrationCorrection_),
      2);                                                                 // 6
   is.read(reinterpret_cast<char*>(&p->dataTransmissionEnabled_), 2);     // 7
   is.read(reinterpret_cast<char*>(&p->volumeCoveragePatternNumber_), 2); // 8
   is.read(reinterpret_cast<char*>(&p->rdaControlAuthorization_), 2);     // 9
   is.read(reinterpret_cast<char*>(&p->rdaBuildNumber_), 2);              // 10
   is.read(reinterpret_cast<char*>(&p->operationalMode_), 2);             // 11
   is.read(reinterpret_cast<char*>(&p->superResolutionStatus_), 2);       // 12
   is.read(reinterpret_cast<char*>(&p->clutterMitigationDecisionStatus_),
           2);                                                         // 13
   is.read(reinterpret_cast<char*>(&p->avsetEbcRdaLogDataStatus_), 2); // 14
   is.read(reinterpret_cast<char*>(&p->rdaAlarmSummary_), 2);          // 15
   is.read(reinterpret_cast<char*>(&p->commandAcknowledgement_), 2);   // 16
   is.read(reinterpret_cast<char*>(&p->channelControlStatus_), 2);     // 17
   is.read(reinterpret_cast<char*>(&p->spotBlankingStatus_), 2);       // 18
   is.read(reinterpret_cast<char*>(&p->bypassMapGenerationDate_), 2);  // 19
   is.read(reinterpret_cast<char*>(&p->bypassMapGenerationTime_), 2);  // 20
   is.read(reinterpret_cast<char*>(&p->clutterFilterMapGenerationDate_),
           2); // 21
   is.read(reinterpret_cast<char*>(&p->clutterFilterMapGenerationTime_),
           2); // 22
   is.read(
      reinterpret_cast<char*>(&p->verticalReflectivityCalibrationCorrection_),
      2);                                                                 // 23
   is.read(reinterpret_cast<char*>(&p->transitionPowerSourceStatus_), 2); // 24
   is.read(reinterpret_cast<char*>(&p->rmsControlStatus_), 2);            // 25
   is.read(reinterpret_cast<char*>(&p->performanceCheckStatus_), 2);      // 26
   is.read(reinterpret_cast<char*>(&p->alarmCodes_),
           p->alarmCodes_.size() * 2);                                // 27-40
   is.read(reinterpret_cast<char*>(&p->signalProcessingOptions_), 2); // 41
   is.seekg(36, std::ios_base::cur);                                  // 42-59
   is.read(reinterpret_cast<char*>(&p->statusVersion_), 2);           // 42
   bytesRead += 120;

   p->rdaStatus_                    = ntohs(p->rdaStatus_);
   p->operabilityStatus_            = ntohs(p->operabilityStatus_);
   p->controlStatus_                = ntohs(p->controlStatus_);
   p->auxiliaryPowerGeneratorState_ = ntohs(p->auxiliaryPowerGeneratorState_);
   p->averageTransmitterPower_      = ntohs(p->averageTransmitterPower_);
   p->horizontalReflectivityCalibrationCorrection_ =
      ntohs(p->horizontalReflectivityCalibrationCorrection_);
   p->dataTransmissionEnabled_     = ntohs(p->dataTransmissionEnabled_);
   p->volumeCoveragePatternNumber_ = ntohs(p->volumeCoveragePatternNumber_);
   p->rdaControlAuthorization_     = ntohs(p->rdaControlAuthorization_);
   p->rdaBuildNumber_              = ntohs(p->rdaBuildNumber_);
   p->operationalMode_             = ntohs(p->operationalMode_);
   p->superResolutionStatus_       = ntohs(p->superResolutionStatus_);
   p->clutterMitigationDecisionStatus_ =
      ntohs(p->clutterMitigationDecisionStatus_);
   p->avsetEbcRdaLogDataStatus_ = ntohs(p->avsetEbcRdaLogDataStatus_);
   p->rdaAlarmSummary_          = ntohs(p->rdaAlarmSummary_);
   p->commandAcknowledgement_   = ntohs(p->commandAcknowledgement_);
   p->channelControlStatus_     = ntohs(p->channelControlStatus_);
   p->spotBlankingStatus_       = ntohs(p->spotBlankingStatus_);
   p->bypassMapGenerationDate_  = ntohs(p->bypassMapGenerationDate_);
   p->bypassMapGenerationTime_  = ntohs(p->bypassMapGenerationTime_);
   p->clutterFilterMapGenerationDate_ =
      ntohs(p->clutterFilterMapGenerationDate_);
   p->clutterFilterMapGenerationTime_ =
      ntohs(p->clutterFilterMapGenerationTime_);
   p->verticalReflectivityCalibrationCorrection_ =
      ntohs(p->verticalReflectivityCalibrationCorrection_);
   p->transitionPowerSourceStatus_ = ntohs(p->transitionPowerSourceStatus_);
   p->rmsControlStatus_            = ntohs(p->rmsControlStatus_);
   p->performanceCheckStatus_      = ntohs(p->performanceCheckStatus_);
   SwapArray(p->alarmCodes_);
   p->signalProcessingOptions_ = ntohs(p->signalProcessingOptions_);
   p->statusVersion_           = ntohs(p->statusVersion_);

   if (!ValidateMessage(is, bytesRead))
   {
      messageValid = false;
   }

   return messageValid;
}

std::shared_ptr<RdaStatusData>
RdaStatusData::Create(Level2MessageHeader&& header, std::istream& is)
{
   std::shared_ptr<RdaStatusData> message = std::make_shared<RdaStatusData>();
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
