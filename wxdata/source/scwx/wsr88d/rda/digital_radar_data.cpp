#include <scwx/wsr88d/rda/digital_radar_data.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

static const std::string logPrefix_ = "scwx::wsr88d::rda::digital_radar_data";
static const auto        logger_    = util::Logger::Create(logPrefix_);

// Table III-A Angle Data Format
constexpr float kAngleDataScale = 0.0054931640625f;

// Table III-B Range Format
constexpr float kRangeScale = 0.001f;

class DigitalRadarData::Impl
{
public:
   explicit Impl() {};
   ~Impl() = default;

   std::uint32_t collectionTime_ {};
   std::uint16_t modifiedJulianDate_ {};
   std::uint16_t unambiguousRange_ {};
   std::uint16_t azimuthAngle_ {};
   std::uint16_t azimuthNumber_ {};
   std::uint16_t radialStatus_ {};
   std::uint16_t elevationAngle_ {};
   std::uint16_t elevationNumber_ {};
   std::uint16_t surveillanceRange_ {};
   std::uint16_t dopplerRange_ {};
   std::uint16_t surveillanceRangeSampleInterval_ {};
   std::uint16_t dopplerRangeSampleInterval_ {};
   std::uint16_t numberOfSurveillanceBins_ {};
   std::uint16_t numberOfDopplerBins_ {};
   std::uint16_t cutSectorNumber_ {};
   float         calibrationConstant_ {};
   std::uint16_t surveillancePointer_ {};
   std::uint16_t velocityPointer_ {};
   std::uint16_t spectralWidthPointer_ {};
   std::uint16_t dopplerVelocityResolution_ {};
   std::uint16_t vcpNumber_ {};
   std::uint16_t nyquistVelocity_ {};
   std::uint16_t atmos_ {};
   std::uint16_t tover_ {};
   std::uint16_t radialSpotBlankingStatus_ {};

   std::vector<std::uint8_t> reflectivity_ {};
   std::vector<std::uint8_t> dopplerVelocity_ {};
   std::vector<std::uint8_t> dopplerSpectrumWidth_ {};
};

DigitalRadarData::DigitalRadarData() :
    Level2Message(), p(std::make_unique<Impl>())
{
}
DigitalRadarData::~DigitalRadarData() = default;

DigitalRadarData::DigitalRadarData(DigitalRadarData&&) noexcept = default;
DigitalRadarData&
DigitalRadarData::operator=(DigitalRadarData&&) noexcept = default;

std::uint32_t DigitalRadarData::collection_time() const
{
   return p->collectionTime_;
}

std::uint16_t DigitalRadarData::modified_julian_date() const
{
   return p->modifiedJulianDate_;
}

std::uint16_t DigitalRadarData::unambiguous_range() const
{
   return p->unambiguousRange_;
}

std::uint16_t DigitalRadarData::azimuth_angle_raw() const
{
   return p->azimuthAngle_;
}

units::degrees<float> DigitalRadarData::azimuth_angle() const
{
   return units::degrees<float> {p->azimuthAngle_ * kAngleDataScale};
}

std::uint16_t DigitalRadarData::azimuth_number() const
{
   return p->azimuthNumber_;
}

std::uint16_t DigitalRadarData::radial_status() const
{
   return p->radialStatus_;
}

std::uint16_t DigitalRadarData::elevation_angle_raw() const
{
   return p->elevationAngle_;
}

units::degrees<float> DigitalRadarData::elevation_angle() const
{
   return units::degrees<float> {p->elevationAngle_ * kAngleDataScale};
}

std::uint16_t DigitalRadarData::elevation_number() const
{
   return p->elevationNumber_;
}

std::uint16_t DigitalRadarData::surveillance_range_raw() const
{
   return p->surveillanceRange_;
}

units::kilometers<float> DigitalRadarData::surveillance_range() const
{
   return units::kilometers<float> {p->surveillanceRange_ * kRangeScale};
}

std::uint16_t DigitalRadarData::doppler_range_raw() const
{
   return p->dopplerRange_;
}

units::kilometers<float> DigitalRadarData::doppler_range() const
{
   return units::kilometers<float> {p->dopplerRange_ * kRangeScale};
}

std::uint16_t DigitalRadarData::surveillance_range_sample_interval_raw() const
{
   return p->surveillanceRangeSampleInterval_;
}

units::kilometers<float>
DigitalRadarData::surveillance_range_sample_interval() const
{
   return units::kilometers<float> {p->surveillanceRangeSampleInterval_ *
                                    kRangeScale};
}

std::uint16_t DigitalRadarData::doppler_range_sample_interval_raw() const
{
   return p->dopplerRangeSampleInterval_;
}

units::kilometers<float> DigitalRadarData::doppler_range_sample_interval() const
{
   return units::kilometers<float> {p->dopplerRangeSampleInterval_ *
                                    kRangeScale};
}

std::uint16_t DigitalRadarData::number_of_surveillance_bins() const
{
   return p->numberOfSurveillanceBins_;
}

std::uint16_t DigitalRadarData::number_of_doppler_bins() const
{
   return p->numberOfDopplerBins_;
}

std::uint16_t DigitalRadarData::cut_sector_number() const
{
   return p->cutSectorNumber_;
}

float DigitalRadarData::calibration_constant() const
{
   return p->calibrationConstant_;
}

std::uint16_t DigitalRadarData::surveillance_pointer() const
{
   return p->surveillancePointer_;
}

std::uint16_t DigitalRadarData::velocity_pointer() const
{
   return p->velocityPointer_;
}

std::uint16_t DigitalRadarData::spectral_width_pointer() const
{
   return p->spectralWidthPointer_;
}

std::uint16_t DigitalRadarData::doppler_velocity_resolution() const
{
   return p->dopplerVelocityResolution_;
}

std::uint16_t DigitalRadarData::volume_coverage_pattern_number() const
{
   return p->vcpNumber_;
}

std::uint16_t DigitalRadarData::nyquist_velocity() const
{
   return p->nyquistVelocity_;
}

std::uint16_t DigitalRadarData::atmos() const
{
   return p->atmos_;
}

std::uint16_t DigitalRadarData::tover() const
{
   return p->tover_;
}

std::uint16_t DigitalRadarData::radial_spot_blanking_status() const
{
   return p->radialSpotBlankingStatus_;
}

bool DigitalRadarData::Parse(std::istream& is)
{
   logger_->trace("Parsing Digital Radar Data (Message Type 1)");

   bool        messageValid = true;
   std::size_t bytesRead    = 0;

   std::streampos isBegin = is.tellg();

   is.read(reinterpret_cast<char*>(&p->collectionTime_), 4);     // 0-3
   is.read(reinterpret_cast<char*>(&p->modifiedJulianDate_), 2); // 4-5
   is.read(reinterpret_cast<char*>(&p->unambiguousRange_), 2);   // 6-7
   is.read(reinterpret_cast<char*>(&p->azimuthAngle_), 2);       // 8-9
   is.read(reinterpret_cast<char*>(&p->azimuthNumber_), 2);      // 10-11
   is.read(reinterpret_cast<char*>(&p->radialStatus_), 2);       // 12-13
   is.read(reinterpret_cast<char*>(&p->elevationAngle_), 2);     // 14-15
   is.read(reinterpret_cast<char*>(&p->elevationNumber_), 2);    // 16-17
   is.read(reinterpret_cast<char*>(&p->surveillanceRange_), 2);  // 18-19
   is.read(reinterpret_cast<char*>(&p->dopplerRange_), 2);       // 20-21

   is.read(reinterpret_cast<char*>(&p->surveillanceRangeSampleInterval_),
           2); // 22-23
   is.read(reinterpret_cast<char*>(&p->dopplerRangeSampleInterval_),
           2); // 24-25

   is.read(reinterpret_cast<char*>(&p->numberOfSurveillanceBins_), 2);  // 26-27
   is.read(reinterpret_cast<char*>(&p->numberOfDopplerBins_), 2);       // 28-29
   is.read(reinterpret_cast<char*>(&p->cutSectorNumber_), 2);           // 30-31
   is.read(reinterpret_cast<char*>(&p->calibrationConstant_), 4);       // 32-35
   is.read(reinterpret_cast<char*>(&p->surveillancePointer_), 2);       // 36-37
   is.read(reinterpret_cast<char*>(&p->velocityPointer_), 2);           // 38-39
   is.read(reinterpret_cast<char*>(&p->spectralWidthPointer_), 2);      // 40-41
   is.read(reinterpret_cast<char*>(&p->dopplerVelocityResolution_), 2); // 42-43
   is.read(reinterpret_cast<char*>(&p->vcpNumber_), 2);                 // 44-45
   is.seekg(14, std::ios_base::cur);                                    // 46-59
   is.read(reinterpret_cast<char*>(&p->nyquistVelocity_), 2);           // 60-61
   is.read(reinterpret_cast<char*>(&p->atmos_), 2);                     // 62-63
   is.read(reinterpret_cast<char*>(&p->tover_), 2);                     // 64-65
   is.read(reinterpret_cast<char*>(&p->radialSpotBlankingStatus_), 2);  // 66-67
   is.seekg(32, std::ios_base::cur);                                    // 68-99

   p->collectionTime_     = ntohl(p->collectionTime_);
   p->modifiedJulianDate_ = ntohs(p->modifiedJulianDate_);
   p->unambiguousRange_   = ntohs(p->unambiguousRange_);
   p->azimuthAngle_       = ntohs(p->azimuthAngle_);
   p->azimuthNumber_      = ntohs(p->azimuthNumber_);
   p->radialStatus_       = ntohs(p->radialStatus_);
   p->elevationAngle_     = ntohs(p->elevationAngle_);
   p->elevationNumber_    = ntohs(p->elevationNumber_);
   p->surveillanceRange_  = ntohs(p->surveillanceRange_);
   p->dopplerRange_       = ntohs(p->dopplerRange_);

   p->surveillanceRangeSampleInterval_ =
      ntohs(p->surveillanceRangeSampleInterval_);

   p->dopplerRangeSampleInterval_ = ntohs(p->dopplerRangeSampleInterval_);
   p->numberOfSurveillanceBins_   = ntohs(p->numberOfSurveillanceBins_);
   p->numberOfDopplerBins_        = ntohs(p->numberOfDopplerBins_);
   p->cutSectorNumber_            = ntohs(p->cutSectorNumber_);
   p->calibrationConstant_        = SwapFloat(p->calibrationConstant_);
   p->surveillancePointer_        = ntohs(p->surveillancePointer_);
   p->velocityPointer_            = ntohs(p->velocityPointer_);
   p->spectralWidthPointer_       = ntohs(p->spectralWidthPointer_);
   p->dopplerVelocityResolution_  = ntohs(p->dopplerVelocityResolution_);
   p->vcpNumber_                  = ntohs(p->vcpNumber_);
   p->nyquistVelocity_            = ntohs(p->nyquistVelocity_);
   p->atmos_                      = ntohs(p->atmos_);
   p->tover_                      = ntohs(p->tover_);
   p->radialSpotBlankingStatus_   = ntohs(p->radialSpotBlankingStatus_);

   if (p->azimuthNumber_ < 1 || p->azimuthNumber_ > 400)
   {
      logger_->warn("Invalid azimuth number: {}", p->azimuthNumber_);
      messageValid = false;
   }
   if (p->elevationNumber_ < 1 || p->elevationNumber_ > 25)
   {
      logger_->warn("Invalid elevation number: {}", p->elevationNumber_);
      messageValid = false;
   }
   if (p->numberOfSurveillanceBins_ > 460)
   {
      logger_->warn("Invalid number of surveillance bins: {}",
                    p->numberOfSurveillanceBins_);
      messageValid = false;
   }
   if (p->numberOfDopplerBins_ > 920)
   {
      logger_->warn("Invalid number of doppler bins: {}",
                    p->numberOfDopplerBins_);
      messageValid = false;
   }
   if (p->surveillancePointer_ != 0 && p->surveillancePointer_ != 100)
   {
      logger_->warn("Invalid surveillance pointer: {}",
                    p->surveillancePointer_);
      messageValid = false;
   }
   if (p->velocityPointer_ != 0 &&
       (p->velocityPointer_ < 100 || p->velocityPointer_ > 560))
   {
      logger_->warn("Invalid velocity pointer: {}", p->velocityPointer_);
      messageValid = false;
   }
   if (p->spectralWidthPointer_ != 0 &&
       (p->spectralWidthPointer_ < 100 || p->spectralWidthPointer_ > 1480 ||
        p->spectralWidthPointer_ > data_size()))
   {
      logger_->warn("Invalid spectral width pointer: {}",
                    p->spectralWidthPointer_);
      messageValid = false;
   }

   if (messageValid && p->surveillancePointer_ != 0)
   {
      is.seekg(isBegin + std::streamoff(p->surveillancePointer_),
               std::ios_base::beg);

      p->reflectivity_.resize(p->numberOfSurveillanceBins_);
      is.read(reinterpret_cast<char*>(p->reflectivity_.data()),
              p->numberOfSurveillanceBins_);
   }

   if (messageValid && p->velocityPointer_ != 0)
   {
      is.seekg(isBegin + std::streamoff(p->velocityPointer_),
               std::ios_base::beg);

      p->dopplerVelocity_.resize(p->numberOfDopplerBins_);
      is.read(reinterpret_cast<char*>(p->dopplerVelocity_.data()),
              p->numberOfDopplerBins_);
   }

   if (messageValid && p->spectralWidthPointer_ != 0)
   {
      is.seekg(isBegin + std::streamoff(p->spectralWidthPointer_),
               std::ios_base::beg);

      p->dopplerSpectrumWidth_.resize(p->numberOfDopplerBins_);
      is.read(reinterpret_cast<char*>(p->dopplerSpectrumWidth_.data()),
              p->numberOfDopplerBins_);
   }

   is.seekg(isBegin, std::ios_base::beg);
   if (!ValidateMessage(is, bytesRead))
   {
      messageValid = false;
   }

   return messageValid;
}

std::shared_ptr<DigitalRadarData>
DigitalRadarData::Create(Level2MessageHeader&& header, std::istream& is)
{
   std::shared_ptr<DigitalRadarData> message =
      std::make_shared<DigitalRadarData>();
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
