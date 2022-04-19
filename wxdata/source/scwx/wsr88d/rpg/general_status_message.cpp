#include <scwx/wsr88d/rpg/general_status_message.hpp>
#include <scwx/util/rangebuf.hpp>

#include <istream>
#include <string>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ =
   "scwx::wsr88d::rpg::general_status_message";

class GeneralStatusMessageImpl
{
public:
   explicit GeneralStatusMessageImpl() :
       blockDivider_ {0},
       lengthOfBlock_ {0},
       modeOfOperation_ {0},
       rdaOperabilityStatus_ {0},
       volumeCoveragePattern_ {0},
       numberOfElevationCuts_ {0},
       elevation_ {0},
       rdaStatus_ {0},
       rdaAlarms_ {0},
       dataTransmissionEnabled_ {0},
       rpgOperabilityStatus_ {0},
       rpgAlarms_ {0},
       rpgStatus_ {0},
       rpgNarrowbandStatus_ {0},
       horizontalReflectivityCalibrationCorrection_ {0},
       productAvailiability_ {0},
       superResolutionElevationCuts_ {0},
       clutterMitigationDecisionStatus_ {0},
       verticalReflectivityCalibrationCorrection_ {0},
       rdaBuildNumber_ {0},
       rdaChannelNumber_ {0},
       buildVersion_ {0},
       vcpSupplementalData_ {0},
       supplementalCutMap_ {0}
   {
   }
   ~GeneralStatusMessageImpl() = default;

   int16_t                 blockDivider_;
   uint16_t                lengthOfBlock_;
   uint16_t                modeOfOperation_;
   uint16_t                rdaOperabilityStatus_;
   uint16_t                volumeCoveragePattern_;
   uint16_t                numberOfElevationCuts_;
   std::array<int16_t, 25> elevation_;
   uint16_t                rdaStatus_;
   uint16_t                rdaAlarms_;
   uint16_t                dataTransmissionEnabled_;
   uint16_t                rpgOperabilityStatus_;
   uint16_t                rpgAlarms_;
   uint16_t                rpgStatus_;
   uint16_t                rpgNarrowbandStatus_;
   int16_t                 horizontalReflectivityCalibrationCorrection_;
   uint16_t                productAvailiability_;
   uint16_t                superResolutionElevationCuts_;
   uint16_t                clutterMitigationDecisionStatus_;
   int16_t                 verticalReflectivityCalibrationCorrection_;
   uint16_t                rdaBuildNumber_;
   uint16_t                rdaChannelNumber_;
   uint16_t                buildVersion_;
   uint16_t                vcpSupplementalData_;
   uint32_t                supplementalCutMap_;
};

GeneralStatusMessage::GeneralStatusMessage() :
    p(std::make_unique<GeneralStatusMessageImpl>())
{
}
GeneralStatusMessage::~GeneralStatusMessage() = default;

GeneralStatusMessage::GeneralStatusMessage(GeneralStatusMessage&&) noexcept =
   default;
GeneralStatusMessage&
GeneralStatusMessage::operator=(GeneralStatusMessage&&) noexcept = default;

int16_t GeneralStatusMessage::block_divider() const
{
   return p->blockDivider_;
}

uint16_t GeneralStatusMessage::length_of_block() const
{
   return p->lengthOfBlock_;
}

uint16_t GeneralStatusMessage::mode_of_operation() const
{
   return p->modeOfOperation_;
}

uint16_t GeneralStatusMessage::rda_operability_status() const
{
   return p->rdaOperabilityStatus_;
}

uint16_t GeneralStatusMessage::volume_coverage_pattern() const
{
   return p->volumeCoveragePattern_;
}

uint16_t GeneralStatusMessage::number_of_elevation_cuts() const
{
   return p->numberOfElevationCuts_;
}

float GeneralStatusMessage::elevation(uint16_t e) const
{
   return p->elevation_[e] * 0.1f;
}

uint16_t GeneralStatusMessage::rda_status() const
{
   return p->rdaStatus_;
}

uint16_t GeneralStatusMessage::rda_alarms() const
{
   return p->rdaAlarms_;
}

uint16_t GeneralStatusMessage::data_transmission_enabled() const
{
   return p->dataTransmissionEnabled_;
}

uint16_t GeneralStatusMessage::rpg_operability_status() const
{
   return p->rpgOperabilityStatus_;
}

uint16_t GeneralStatusMessage::rpg_alarms() const
{
   return p->rpgAlarms_;
}

uint16_t GeneralStatusMessage::rpg_status() const
{
   return p->rpgStatus_;
}

uint16_t GeneralStatusMessage::rpg_narrowband_status() const
{
   return p->rpgNarrowbandStatus_;
}

float GeneralStatusMessage::horizontal_reflectivity_calibration_correction()
   const
{
   return p->horizontalReflectivityCalibrationCorrection_ * 0.25f;
}

uint16_t GeneralStatusMessage::product_availiability() const
{
   return p->productAvailiability_;
}

uint16_t GeneralStatusMessage::super_resolution_elevation_cuts() const
{
   return p->superResolutionElevationCuts_;
}

uint16_t GeneralStatusMessage::clutter_mitigation_decision_status() const
{
   return p->clutterMitigationDecisionStatus_;
}

float GeneralStatusMessage::vertical_reflectivity_calibration_correction() const
{
   return p->verticalReflectivityCalibrationCorrection_ * 0.25f;
}

uint16_t GeneralStatusMessage::rda_build_number() const
{
   return p->rdaBuildNumber_;
}

uint16_t GeneralStatusMessage::rda_channel_number() const
{
   return p->rdaChannelNumber_;
}

uint16_t GeneralStatusMessage::build_version() const
{
   return p->buildVersion_;
}

uint16_t GeneralStatusMessage::vcp_supplemental_data() const
{
   return p->vcpSupplementalData_;
}

uint32_t GeneralStatusMessage::supplemental_cut_map() const
{
   return p->supplementalCutMap_;
}

bool GeneralStatusMessage::Parse(std::istream& is)
{
   bool dataValid = true;

   const std::streampos dataStart = is.tellg();

   is.read(reinterpret_cast<char*>(&p->blockDivider_), 2);
   is.read(reinterpret_cast<char*>(&p->lengthOfBlock_), 2);
   is.read(reinterpret_cast<char*>(&p->modeOfOperation_), 2);
   is.read(reinterpret_cast<char*>(&p->rdaOperabilityStatus_), 2);
   is.read(reinterpret_cast<char*>(&p->volumeCoveragePattern_), 2);
   is.read(reinterpret_cast<char*>(&p->numberOfElevationCuts_), 2);
   is.read(reinterpret_cast<char*>(p->elevation_.data()), 40);
   is.read(reinterpret_cast<char*>(&p->rdaStatus_), 2);
   is.read(reinterpret_cast<char*>(&p->rdaAlarms_), 2);
   is.read(reinterpret_cast<char*>(&p->dataTransmissionEnabled_), 2);
   is.read(reinterpret_cast<char*>(&p->rpgOperabilityStatus_), 2);
   is.read(reinterpret_cast<char*>(&p->rpgAlarms_), 2);
   is.read(reinterpret_cast<char*>(&p->rpgStatus_), 2);
   is.read(reinterpret_cast<char*>(&p->rpgNarrowbandStatus_), 2);
   is.read(
      reinterpret_cast<char*>(&p->horizontalReflectivityCalibrationCorrection_),
      2);
   is.read(reinterpret_cast<char*>(&p->productAvailiability_), 2);
   is.read(reinterpret_cast<char*>(&p->superResolutionElevationCuts_), 2);
   is.read(reinterpret_cast<char*>(&p->clutterMitigationDecisionStatus_), 2);
   is.read(
      reinterpret_cast<char*>(&p->verticalReflectivityCalibrationCorrection_),
      2);
   is.read(reinterpret_cast<char*>(&p->rdaBuildNumber_), 2);
   is.read(reinterpret_cast<char*>(&p->rdaChannelNumber_), 2);
   is.seekg(4, std::ios_base::cur);
   is.read(reinterpret_cast<char*>(&p->buildVersion_), 2);
   is.read(reinterpret_cast<char*>(&p->elevation_[20]), 10);
   is.read(reinterpret_cast<char*>(&p->vcpSupplementalData_), 2);
   is.read(reinterpret_cast<char*>(&p->supplementalCutMap_), 4);
   is.seekg(80, std::ios_base::cur);

   p->blockDivider_            = ntohs(p->blockDivider_);
   p->lengthOfBlock_           = ntohs(p->lengthOfBlock_);
   p->modeOfOperation_         = ntohs(p->modeOfOperation_);
   p->rdaOperabilityStatus_    = ntohs(p->rdaOperabilityStatus_);
   p->volumeCoveragePattern_   = ntohs(p->volumeCoveragePattern_);
   p->numberOfElevationCuts_   = ntohs(p->numberOfElevationCuts_);
   p->rdaStatus_               = ntohs(p->rdaStatus_);
   p->rdaAlarms_               = ntohs(p->rdaAlarms_);
   p->dataTransmissionEnabled_ = ntohs(p->dataTransmissionEnabled_);
   p->rpgOperabilityStatus_    = ntohs(p->rpgOperabilityStatus_);
   p->rpgAlarms_               = ntohs(p->rpgAlarms_);
   p->rpgStatus_               = ntohs(p->rpgStatus_);
   p->rpgNarrowbandStatus_     = ntohs(p->rpgNarrowbandStatus_);
   p->horizontalReflectivityCalibrationCorrection_ =
      ntohs(p->horizontalReflectivityCalibrationCorrection_);
   p->productAvailiability_         = ntohs(p->productAvailiability_);
   p->superResolutionElevationCuts_ = ntohs(p->superResolutionElevationCuts_);
   p->clutterMitigationDecisionStatus_ =
      ntohs(p->clutterMitigationDecisionStatus_);
   p->verticalReflectivityCalibrationCorrection_ =
      ntohs(p->verticalReflectivityCalibrationCorrection_);
   p->rdaBuildNumber_      = ntohs(p->rdaBuildNumber_);
   p->rdaChannelNumber_    = ntohs(p->rdaChannelNumber_);
   p->buildVersion_        = ntohs(p->buildVersion_);
   p->vcpSupplementalData_ = ntohs(p->vcpSupplementalData_);
   p->supplementalCutMap_  = ntohl(p->supplementalCutMap_);

   SwapArray(p->elevation_);

   const std::streampos dataEnd = is.tellg();
   if (!ValidateMessage(is, dataEnd - dataStart))
   {
      dataValid = false;
   }

   return dataValid;
}

std::shared_ptr<GeneralStatusMessage>
GeneralStatusMessage::Create(Level3MessageHeader&& header, std::istream& is)
{
   std::shared_ptr<GeneralStatusMessage> message =
      std::make_shared<GeneralStatusMessage>();
   message->set_header(std::move(header));

   if (!message->Parse(is))
   {
      message.reset();
   }

   return message;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
