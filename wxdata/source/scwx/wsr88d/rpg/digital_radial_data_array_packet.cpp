#include <scwx/wsr88d/rpg/digital_radial_data_array_packet.hpp>
#include <scwx/util/logger.hpp>

#include <istream>
#include <string>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ =
   "scwx::wsr88d::rpg::digital_radial_data_array_packet";
static const auto logger_ = util::Logger::Create(logPrefix_);

class DigitalRadialDataArrayPacketImpl
{
public:
   struct Radial
   {
      uint16_t             numberOfBytes_;
      uint16_t             startAngle_;
      uint16_t             deltaAngle_;
      std::vector<uint8_t> level_;

      Radial() : numberOfBytes_ {0}, startAngle_ {0}, deltaAngle_ {0}, level_ {}
      {
      }
   };

   explicit DigitalRadialDataArrayPacketImpl() :
       packetCode_ {0},
       indexOfFirstRangeBin_ {0},
       numberOfRangeBins_ {0},
       iCenterOfSweep_ {0},
       jCenterOfSweep_ {0},
       rangeScaleFactor_ {0},
       radial_ {},
       dataSize_ {0}
   {
   }
   ~DigitalRadialDataArrayPacketImpl() = default;

   uint16_t packetCode_;
   uint16_t indexOfFirstRangeBin_;
   uint16_t numberOfRangeBins_;
   int16_t  iCenterOfSweep_;
   int16_t  jCenterOfSweep_;
   uint16_t rangeScaleFactor_;
   uint16_t numberOfRadials_;

   // Repeat for each radial
   std::vector<Radial> radial_;

   size_t dataSize_;
};

DigitalRadialDataArrayPacket::DigitalRadialDataArrayPacket() :
    p(std::make_unique<DigitalRadialDataArrayPacketImpl>())
{
}
DigitalRadialDataArrayPacket::~DigitalRadialDataArrayPacket() = default;

DigitalRadialDataArrayPacket::DigitalRadialDataArrayPacket(
   DigitalRadialDataArrayPacket&&) noexcept                         = default;
DigitalRadialDataArrayPacket& DigitalRadialDataArrayPacket::operator=(
   DigitalRadialDataArrayPacket&&) noexcept = default;

uint16_t DigitalRadialDataArrayPacket::packet_code() const
{
   return p->packetCode_;
}

uint16_t DigitalRadialDataArrayPacket::index_of_first_range_bin() const
{
   return p->indexOfFirstRangeBin_;
}

uint16_t DigitalRadialDataArrayPacket::number_of_range_bins() const
{
   return p->numberOfRangeBins_;
}

int16_t DigitalRadialDataArrayPacket::i_center_of_sweep() const
{
   return p->iCenterOfSweep_;
}

int16_t DigitalRadialDataArrayPacket::j_center_of_sweep() const
{
   return p->jCenterOfSweep_;
}

float DigitalRadialDataArrayPacket::range_scale_factor() const
{
   return p->rangeScaleFactor_ * 0.001f;
}

uint16_t DigitalRadialDataArrayPacket::number_of_radials() const
{
   return p->numberOfRadials_;
}

size_t DigitalRadialDataArrayPacket::data_size() const
{
   return p->dataSize_;
}

float DigitalRadialDataArrayPacket::start_angle(uint16_t r) const
{
   return p->radial_[r].startAngle_ * 0.1f;
}

float DigitalRadialDataArrayPacket::delta_angle(uint16_t r) const
{
   return p->radial_[r].deltaAngle_ * 0.1f;
}

const std::vector<uint8_t>&
DigitalRadialDataArrayPacket::level(uint16_t r) const
{
   return p->radial_[r].level_;
}

bool DigitalRadialDataArrayPacket::Parse(std::istream& is)
{
   bool   blockValid = true;
   size_t bytesRead  = 0;

   is.read(reinterpret_cast<char*>(&p->packetCode_), 2);
   is.read(reinterpret_cast<char*>(&p->indexOfFirstRangeBin_), 2);
   is.read(reinterpret_cast<char*>(&p->numberOfRangeBins_), 2);
   is.read(reinterpret_cast<char*>(&p->iCenterOfSweep_), 2);
   is.read(reinterpret_cast<char*>(&p->jCenterOfSweep_), 2);
   is.read(reinterpret_cast<char*>(&p->rangeScaleFactor_), 2);
   is.read(reinterpret_cast<char*>(&p->numberOfRadials_), 2);
   bytesRead += 14;

   p->packetCode_           = ntohs(p->packetCode_);
   p->indexOfFirstRangeBin_ = ntohs(p->indexOfFirstRangeBin_);
   p->numberOfRangeBins_    = ntohs(p->numberOfRangeBins_);
   p->iCenterOfSweep_       = ntohs(p->iCenterOfSweep_);
   p->jCenterOfSweep_       = ntohs(p->jCenterOfSweep_);
   p->rangeScaleFactor_     = ntohs(p->rangeScaleFactor_);
   p->numberOfRadials_      = ntohs(p->numberOfRadials_);

   if (is.eof())
   {
      logger_->debug("Reached end of file");
      blockValid = false;
   }
   else
   {
      if (p->packetCode_ != 16)
      {
         logger_->warn("Invalid packet code: {}", p->packetCode_);
         blockValid = false;
      }
      if (p->indexOfFirstRangeBin_ < 0 || p->indexOfFirstRangeBin_ > 230)
      {
         logger_->warn("Invalid index of first range bin: {}",
                       p->indexOfFirstRangeBin_);
         blockValid = false;
      }
      if (p->numberOfRangeBins_ < 0 || p->numberOfRangeBins_ > 1840)
      {
         logger_->warn("Invalid number of range bins: {}",
                       p->numberOfRangeBins_);
         blockValid = false;
      }
      if (p->numberOfRadials_ < 1 || p->numberOfRadials_ > 720)
      {
         logger_->warn("Invalid number of radials: {}", p->numberOfRadials_);
         blockValid = false;
      }
   }

   if (blockValid)
   {
      p->radial_.resize(p->numberOfRadials_);

      for (uint16_t r = 0; r < p->numberOfRadials_; r++)
      {
         auto& radial = p->radial_[r];

         is.read(reinterpret_cast<char*>(&radial.numberOfBytes_), 2);
         is.read(reinterpret_cast<char*>(&radial.startAngle_), 2);
         is.read(reinterpret_cast<char*>(&radial.deltaAngle_), 2);
         bytesRead += 6;

         radial.numberOfBytes_ = ntohs(radial.numberOfBytes_);
         radial.startAngle_    = ntohs(radial.startAngle_);
         radial.deltaAngle_    = ntohs(radial.deltaAngle_);

         if (radial.numberOfBytes_ < 1 || radial.numberOfBytes_ > 1840)
         {
            logger_->warn("Invalid number of bytes: {} (Radial {})",
                          radial.numberOfBytes_,
                          r);
            blockValid = false;
            break;
         }
         else if (radial.numberOfBytes_ < p->numberOfRangeBins_)
         {
            logger_->warn(
               "Number of bytes < number of range bins: {} < {} (Radial {})",
               radial.numberOfBytes_,
               p->numberOfRangeBins_,
               r);
            blockValid = false;
            break;
         }

         // Read radial bins
         size_t dataSize = p->numberOfRangeBins_;
         radial.level_.resize(dataSize);
         is.read(reinterpret_cast<char*>(radial.level_.data()), dataSize);

         is.seekg(radial.numberOfBytes_ - dataSize, std::ios_base::cur);
         bytesRead += radial.numberOfBytes_;
      }
   }

   p->dataSize_ = bytesRead;

   if (!ValidateMessage(is, bytesRead))
   {
      blockValid = false;
   }

   return blockValid;
}

std::shared_ptr<DigitalRadialDataArrayPacket>
DigitalRadialDataArrayPacket::Create(std::istream& is)
{
   std::shared_ptr<DigitalRadialDataArrayPacket> packet =
      std::make_shared<DigitalRadialDataArrayPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
