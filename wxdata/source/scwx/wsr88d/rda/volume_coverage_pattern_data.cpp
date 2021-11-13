#include <scwx/wsr88d/rda/volume_coverage_pattern_data.hpp>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

static const std::string logPrefix_ =
   "[scwx::wsr88d::rda::volume_coverage_pattern_data] ";

struct Sector;

static void ReadSector(std::istream& is, Sector& s);
static void SwapSector(Sector& s);

struct Sector
{
   uint16_t edgeAngle_;
   uint16_t dopplerPrfNumber_;
   uint16_t dopplerPrfPulseCountRadial_;

   Sector() :
       edgeAngle_ {0}, dopplerPrfNumber_ {0}, dopplerPrfPulseCountRadial_ {0}
   {
   }
};

struct ElevationCut
{
   uint16_t              elevationAngle_;
   uint8_t               channelConfiguration_;
   uint8_t               waveformType_;
   uint8_t               superResolutionControl_;
   uint8_t               surveillancePrfNumber_;
   uint16_t              surveillancePrfPulseCountRadial_;
   int16_t               azimuthRate_;
   uint16_t              reflectivityThreshold_;
   uint16_t              velocityThreshold_;
   uint16_t              spectrumWidthThreshold_;
   uint16_t              differentialReflectivityThreshold_;
   uint16_t              differentialPhaseThreshold_;
   uint16_t              correlationCoefficientThreshold_;
   std::array<Sector, 3> sector_;
   uint16_t              supplementalData_;
   uint16_t              ebcAngle_;

   ElevationCut() :
       elevationAngle_ {0},
       channelConfiguration_ {0},
       waveformType_ {0},
       superResolutionControl_ {0},
       surveillancePrfNumber_ {0},
       surveillancePrfPulseCountRadial_ {0},
       azimuthRate_ {0},
       reflectivityThreshold_ {0},
       velocityThreshold_ {0},
       spectrumWidthThreshold_ {0},
       differentialReflectivityThreshold_ {0},
       differentialPhaseThreshold_ {0},
       correlationCoefficientThreshold_ {0},
       sector_(),
       supplementalData_ {0},
       ebcAngle_ {0}
   {
   }
};

class VolumeCoveragePatternDataImpl
{
public:
   explicit VolumeCoveragePatternDataImpl() :
       patternType_ {0},
       patternNumber_ {0},
       version_ {0},
       clutterMapGroupNumber_ {0},
       dopplerVelocityResolution_ {0},
       pulseWidth_ {0},
       vcpSequencing_ {0},
       vcpSupplementalData_ {0},
       elevationCuts_() {};
   ~VolumeCoveragePatternDataImpl() = default;

   uint16_t                  patternType_;
   uint16_t                  patternNumber_;
   uint8_t                   version_;
   uint8_t                   clutterMapGroupNumber_;
   uint8_t                   dopplerVelocityResolution_;
   uint8_t                   pulseWidth_;
   uint16_t                  vcpSequencing_;
   uint16_t                  vcpSupplementalData_;
   std::vector<ElevationCut> elevationCuts_;
};

VolumeCoveragePatternData::VolumeCoveragePatternData() :
    Message(), p(std::make_unique<VolumeCoveragePatternDataImpl>())
{
}
VolumeCoveragePatternData::~VolumeCoveragePatternData() = default;

VolumeCoveragePatternData::VolumeCoveragePatternData(
   VolumeCoveragePatternData&&) noexcept                      = default;
VolumeCoveragePatternData& VolumeCoveragePatternData::operator=(
   VolumeCoveragePatternData&&) noexcept = default;

uint16_t VolumeCoveragePatternData::pattern_type() const
{
   return p->patternType_;
}

uint16_t VolumeCoveragePatternData::pattern_number() const
{
   return p->patternNumber_;
}

uint16_t VolumeCoveragePatternData::number_of_elevation_cuts() const
{
   return static_cast<uint16_t>(p->elevationCuts_.size());
}

uint8_t VolumeCoveragePatternData::version() const
{
   return p->version_;
}

uint8_t VolumeCoveragePatternData::clutter_map_group_number() const
{
   return p->clutterMapGroupNumber_;
}

float VolumeCoveragePatternData::doppler_velocity_resolution() const
{
   float resolution = 0.0f;

   switch (p->dopplerVelocityResolution_)
   {
   case 2: resolution = 0.5f; break;
   case 4: resolution = 1.0f; break;
   }

   return resolution;
}

uint8_t VolumeCoveragePatternData::pulse_width() const
{
   return p->pulseWidth_;
}

uint16_t VolumeCoveragePatternData::vcp_sequencing() const
{
   return p->vcpSequencing_;
}

uint16_t VolumeCoveragePatternData::number_of_elevations() const
{
   return p->vcpSequencing_ & 0x001f;
}

uint16_t VolumeCoveragePatternData::maximum_sails_cuts() const
{
   return (p->vcpSequencing_ & 0x0060) >> 5;
}

bool VolumeCoveragePatternData::sequence_active() const
{
   return p->vcpSequencing_ & 0x2000;
}

bool VolumeCoveragePatternData::truncated_vcp() const
{
   return p->vcpSequencing_ & 0x4000;
}

uint16_t VolumeCoveragePatternData::vcp_supplemental_data() const
{
   return p->vcpSupplementalData_;
}

bool VolumeCoveragePatternData::sails_vcp() const
{
   return p->vcpSupplementalData_ & 0x0001;
}

uint16_t VolumeCoveragePatternData::number_of_sails_cuts() const
{
   return (p->vcpSupplementalData_ & 0x000E) >> 1;
}

bool VolumeCoveragePatternData::mrle_vcp() const
{
   return p->vcpSupplementalData_ & 0x0010;
}

uint16_t VolumeCoveragePatternData::number_of_mrle_cuts() const
{
   return (p->vcpSupplementalData_ & 0x00E0) >> 5;
}

bool VolumeCoveragePatternData::mpda_vcp() const
{
   return p->vcpSupplementalData_ & 0x0800;
}

bool VolumeCoveragePatternData::base_tilt_vcp() const
{
   return p->vcpSupplementalData_ & 0x1000;
}

uint16_t VolumeCoveragePatternData::number_of_base_tilts() const
{
   return (p->vcpSupplementalData_ & 0xE000) >> 13;
}

double VolumeCoveragePatternData::elevation_angle(uint16_t e) const
{
   return p->elevationCuts_[e].elevationAngle_ * ANGLE_DATA_SCALE;
}

uint16_t VolumeCoveragePatternData::elevation_angle_raw(uint16_t e) const
{
   return p->elevationCuts_[e].elevationAngle_;
}

uint8_t VolumeCoveragePatternData::channel_configuration(uint16_t e) const
{
   return p->elevationCuts_[e].channelConfiguration_;
}

WaveformType VolumeCoveragePatternData::waveform_type(uint16_t e) const
{
   switch (p->elevationCuts_[e].waveformType_)
   {
   case 1: return WaveformType::ContiguousSurveillance;
   case 2: return WaveformType::ContiguousDopplerWithAmbiguityResolution;
   case 3: return WaveformType::ContiguousDopplerWithoutAmbiguityResolution;
   case 4: return WaveformType::Batch;
   case 5: return WaveformType::StaggeredPulsePair;
   default: return WaveformType::Unknown;
   }
}

uint8_t VolumeCoveragePatternData::waveform_type_raw(uint16_t e) const
{
   return p->elevationCuts_[e].waveformType_;
}

uint8_t VolumeCoveragePatternData::super_resolution_control(uint16_t e) const
{
   return p->elevationCuts_[e].superResolutionControl_;
}

bool VolumeCoveragePatternData::half_degree_azimuth(uint16_t e) const
{
   return p->elevationCuts_[e].superResolutionControl_ & 0x01;
}

bool VolumeCoveragePatternData::quarter_km_reflectivity(uint16_t e) const
{
   return p->elevationCuts_[e].superResolutionControl_ & 0x02;
}

bool VolumeCoveragePatternData::doppler_to_300km(uint16_t e) const
{
   return p->elevationCuts_[e].superResolutionControl_ & 0x04;
}

bool VolumeCoveragePatternData::dual_polarization_to_300km(uint16_t e) const
{
   return p->elevationCuts_[e].superResolutionControl_ & 0x08;
}

uint8_t VolumeCoveragePatternData::surveillance_prf_number(uint16_t e) const
{
   return p->elevationCuts_[e].surveillancePrfNumber_;
}

uint16_t
VolumeCoveragePatternData::surveillance_prf_pulse_count_radial(uint16_t e) const
{
   return p->elevationCuts_[e].surveillancePrfPulseCountRadial_;
}

double VolumeCoveragePatternData::azimuth_rate(uint16_t e) const
{
   return p->elevationCuts_[e].azimuthRate_ * AZ_EL_RATE_DATA_SCALE;
}

float VolumeCoveragePatternData::reflectivity_threshold(uint16_t e) const
{
   return p->elevationCuts_[e].reflectivityThreshold_ * 0.125f;
}

float VolumeCoveragePatternData::velocity_threshold(uint16_t e) const
{
   return p->elevationCuts_[e].velocityThreshold_ * 0.125f;
}

float VolumeCoveragePatternData::spectrum_width_threshold(uint16_t e) const
{
   return p->elevationCuts_[e].spectrumWidthThreshold_ * 0.125f;
}

float VolumeCoveragePatternData::differential_reflectivity_threshold(
   uint16_t e) const
{
   return p->elevationCuts_[e].differentialReflectivityThreshold_ * 0.125f;
}

float VolumeCoveragePatternData::differential_phase_threshold(uint16_t e) const
{
   return p->elevationCuts_[e].differentialPhaseThreshold_ * 0.125f;
}

float VolumeCoveragePatternData::correlation_coefficient_threshold(
   uint16_t e) const
{
   return p->elevationCuts_[e].correlationCoefficientThreshold_ * 0.125f;
}

uint16_t VolumeCoveragePatternData::supplemental_data(uint16_t e) const
{
   return p->elevationCuts_[e].supplementalData_;
}

bool VolumeCoveragePatternData::sails_cut(uint16_t e) const
{
   return p->elevationCuts_[e].supplementalData_ & 0x0001;
}

uint16_t VolumeCoveragePatternData::sails_sequence_number(uint16_t e) const
{
   return (p->elevationCuts_[e].supplementalData_ & 0x000E) >> 1;
}

bool VolumeCoveragePatternData::mrle_cut(uint16_t e) const
{
   return p->elevationCuts_[e].supplementalData_ & 0x0010;
}

uint16_t VolumeCoveragePatternData::mrle_sequence_number(uint16_t e) const
{
   return (p->elevationCuts_[e].supplementalData_ & 0x00E0) >> 5;
}

bool VolumeCoveragePatternData::mpda_cut(uint16_t e) const
{
   return p->elevationCuts_[e].supplementalData_ & 0x0200;
}

bool VolumeCoveragePatternData::base_tilt_cut(uint16_t e) const
{
   return p->elevationCuts_[e].supplementalData_ & 0x0400;
}

double VolumeCoveragePatternData::ebc_angle(uint16_t e) const
{
   return p->elevationCuts_[e].ebcAngle_ * ANGLE_DATA_SCALE;
}

double VolumeCoveragePatternData::edge_angle(uint16_t e, uint16_t s) const
{
   return p->elevationCuts_[e].sector_[s].edgeAngle_ * ANGLE_DATA_SCALE;
}

uint16_t VolumeCoveragePatternData::doppler_prf_number(uint16_t e,
                                                       uint16_t s) const
{
   return p->elevationCuts_[e].sector_[s].dopplerPrfNumber_;
}

uint16_t
VolumeCoveragePatternData::doppler_prf_pulse_count_radial(uint16_t e,
                                                          uint16_t s) const
{
   return p->elevationCuts_[e].sector_[s].dopplerPrfPulseCountRadial_;
}

bool VolumeCoveragePatternData::Parse(std::istream& is)
{
   BOOST_LOG_TRIVIAL(trace)
      << logPrefix_ << "Parsing Volume Coverage Pattern Data (Message Type 5)";

   bool   messageValid = true;
   size_t bytesRead    = 0;

   uint16_t messageSize           = 0;
   uint16_t numberOfElevationCuts = 0;

   is.read(reinterpret_cast<char*>(&messageSize), 2);                   // 1
   is.read(reinterpret_cast<char*>(&p->patternType_), 2);               // 2
   is.read(reinterpret_cast<char*>(&p->patternNumber_), 2);             // 3
   is.read(reinterpret_cast<char*>(&numberOfElevationCuts), 2);         // 4
   is.read(reinterpret_cast<char*>(&p->version_), 1);                   // 5
   is.read(reinterpret_cast<char*>(&p->clutterMapGroupNumber_), 1);     // 5
   is.read(reinterpret_cast<char*>(&p->dopplerVelocityResolution_), 1); // 6
   is.read(reinterpret_cast<char*>(&p->pulseWidth_), 1);                // 6
   is.seekg(4, std::ios_base::cur);                                     // 7-8
   is.read(reinterpret_cast<char*>(&p->vcpSequencing_), 2);             // 9
   is.read(reinterpret_cast<char*>(&p->vcpSupplementalData_), 2);       // 10
   is.seekg(2, std::ios_base::cur);                                     // 11
   bytesRead += 22;

   messageSize             = ntohs(messageSize);
   p->patternType_         = ntohs(p->patternType_);
   p->patternNumber_       = ntohs(p->patternNumber_);
   numberOfElevationCuts   = ntohs(numberOfElevationCuts);
   p->vcpSequencing_       = ntohs(p->vcpSequencing_);
   p->vcpSupplementalData_ = ntohs(p->vcpSupplementalData_);

   if (messageSize < 34 || messageSize > 747)
   {
      BOOST_LOG_TRIVIAL(warning)
         << logPrefix_ << "Invalid message size: " << messageSize;
      messageValid = false;
   }
   if (numberOfElevationCuts < 1 || numberOfElevationCuts > 32)
   {
      BOOST_LOG_TRIVIAL(warning)
         << logPrefix_
         << "Invalid number of elevation cuts: " << numberOfElevationCuts;
      messageValid = false;
   }

   if (!messageValid)
   {
      numberOfElevationCuts = 0;
   }

   p->elevationCuts_.resize(numberOfElevationCuts);

   for (uint16_t e = 0; e < numberOfElevationCuts; ++e)
   {
      ElevationCut& c = p->elevationCuts_[e];

      is.read(reinterpret_cast<char*>(&c.elevationAngle_), 2);         // E1
      is.read(reinterpret_cast<char*>(&c.channelConfiguration_), 1);   // E2
      is.read(reinterpret_cast<char*>(&c.waveformType_), 1);           // E2
      is.read(reinterpret_cast<char*>(&c.superResolutionControl_), 1); // E3
      is.read(reinterpret_cast<char*>(&c.surveillancePrfNumber_), 1);  // E3
      is.read(reinterpret_cast<char*>(&c.surveillancePrfPulseCountRadial_),
              2);                                                      // E4
      is.read(reinterpret_cast<char*>(&c.azimuthRate_), 2);            // E5
      is.read(reinterpret_cast<char*>(&c.reflectivityThreshold_), 2);  // E6
      is.read(reinterpret_cast<char*>(&c.velocityThreshold_), 2);      // E7
      is.read(reinterpret_cast<char*>(&c.spectrumWidthThreshold_), 2); // E8
      is.read(reinterpret_cast<char*>(&c.differentialReflectivityThreshold_),
              2); // E9
      is.read(reinterpret_cast<char*>(&c.differentialPhaseThreshold_),
              2); // E10
      is.read(reinterpret_cast<char*>(&c.correlationCoefficientThreshold_),
              2);                                                // E11
      ReadSector(is, c.sector_[0]);                              // E12-E14
      is.read(reinterpret_cast<char*>(&c.supplementalData_), 2); // E15
      ReadSector(is, c.sector_[1]);                              // E16-E18
      is.read(reinterpret_cast<char*>(&c.ebcAngle_), 2);         // E19
      ReadSector(is, c.sector_[2]);                              // E20-E22
      is.seekg(2, std::ios_base::cur);                           // E23
      bytesRead += 46;

      c.elevationAngle_ = ntohs(c.elevationAngle_);
      c.surveillancePrfPulseCountRadial_ =
         ntohs(c.surveillancePrfPulseCountRadial_);
      c.azimuthRate_            = ntohs(c.azimuthRate_);
      c.reflectivityThreshold_  = ntohs(c.reflectivityThreshold_);
      c.velocityThreshold_      = ntohs(c.velocityThreshold_);
      c.spectrumWidthThreshold_ = ntohs(c.spectrumWidthThreshold_);
      c.differentialReflectivityThreshold_ =
         ntohs(c.differentialReflectivityThreshold_);
      c.differentialPhaseThreshold_ = ntohs(c.differentialPhaseThreshold_);
      c.correlationCoefficientThreshold_ =
         ntohs(c.correlationCoefficientThreshold_);
      c.supplementalData_ = ntohs(c.supplementalData_);
      c.ebcAngle_         = ntohs(c.ebcAngle_);

      for (size_t s = 0; s < c.sector_.size(); s++)
      {
         SwapSector(c.sector_[s]);
      }
   }

   if (messageValid && bytesRead != messageSize * 2)
   {
      BOOST_LOG_TRIVIAL(warning)
         << logPrefix_ << "Bytes read (" << bytesRead
         << ") not equal to message size (" << messageSize * 2 << ")";
   }

   if (!ValidateMessage(is, bytesRead))
   {
      messageValid = false;
   }

   if (!messageValid)
   {
      p->elevationCuts_.resize(0);
      p->elevationCuts_.shrink_to_fit();
   }

   return messageValid;
}

std::shared_ptr<VolumeCoveragePatternData>
VolumeCoveragePatternData::Create(MessageHeader&& header, std::istream& is)
{
   std::shared_ptr<VolumeCoveragePatternData> message =
      std::make_shared<VolumeCoveragePatternData>();
   message->set_header(std::move(header));

   if (!message->Parse(is))
   {
      message.reset();
   }

   return message;
}

static void ReadSector(std::istream& is, Sector& s)
{
   is.read(reinterpret_cast<char*>(&s.edgeAngle_), 2);                  // S1
   is.read(reinterpret_cast<char*>(&s.dopplerPrfNumber_), 2);           // S2
   is.read(reinterpret_cast<char*>(&s.dopplerPrfPulseCountRadial_), 2); // S3
}

static void SwapSector(Sector& s)
{
   s.edgeAngle_                  = ntohs(s.edgeAngle_);
   s.dopplerPrfNumber_           = ntohs(s.dopplerPrfNumber_);
   s.dopplerPrfPulseCountRadial_ = ntohs(s.dopplerPrfPulseCountRadial_);
}

} // namespace rda
} // namespace wsr88d
} // namespace scwx
