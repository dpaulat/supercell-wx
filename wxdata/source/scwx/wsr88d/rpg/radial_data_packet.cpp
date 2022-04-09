#include <scwx/wsr88d/rpg/radial_data_packet.hpp>

#include <istream>
#include <string>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ =
   "[scwx::wsr88d::rpg::radial_data_packet] ";

class RadialDataPacketImpl
{
public:
   struct Radial
   {
      uint16_t             numberOfRleHalfwords_;
      uint16_t             startAngle_;
      uint16_t             angleDelta_;
      std::vector<uint8_t> data_;
      std::vector<uint8_t> level_;

      Radial() :
          numberOfRleHalfwords_ {0},
          startAngle_ {0},
          angleDelta_ {0},
          data_ {},
          level_ {}
      {
      }
   };

   explicit RadialDataPacketImpl() :
       packetCode_ {0},
       indexOfFirstRangeBin_ {0},
       numberOfRangeBins_ {0},
       iCenterOfSweep_ {0},
       jCenterOfSweep_ {0},
       scaleFactor_ {0},
       radial_ {},
       dataSize_ {0}
   {
   }
   ~RadialDataPacketImpl() = default;

   uint16_t packetCode_;
   uint16_t indexOfFirstRangeBin_;
   uint16_t numberOfRangeBins_;
   int16_t  iCenterOfSweep_;
   int16_t  jCenterOfSweep_;
   uint16_t scaleFactor_;
   uint16_t numberOfRadials_;

   // Repeat for each radial
   std::vector<Radial> radial_;

   size_t dataSize_;
};

RadialDataPacket::RadialDataPacket() :
    p(std::make_unique<RadialDataPacketImpl>())
{
}
RadialDataPacket::~RadialDataPacket() = default;

RadialDataPacket::RadialDataPacket(RadialDataPacket&&) noexcept = default;
RadialDataPacket&
RadialDataPacket::operator=(RadialDataPacket&&) noexcept = default;

uint16_t RadialDataPacket::packet_code() const
{
   return p->packetCode_;
}

uint16_t RadialDataPacket::index_of_first_range_bin() const
{
   return p->indexOfFirstRangeBin_;
}

uint16_t RadialDataPacket::number_of_range_bins() const
{
   return p->numberOfRangeBins_;
}

int16_t RadialDataPacket::i_center_of_sweep() const
{
   return p->iCenterOfSweep_;
}

int16_t RadialDataPacket::j_center_of_sweep() const
{
   return p->jCenterOfSweep_;
}

float RadialDataPacket::scale_factor() const
{
   return p->scaleFactor_ * 0.001f;
}

uint16_t RadialDataPacket::number_of_radials() const
{
   return p->numberOfRadials_;
}

float RadialDataPacket::start_angle(uint16_t r) const
{
   return p->radial_[r].startAngle_ * 0.1f;
}

float RadialDataPacket::delta_angle(uint16_t r) const
{
   return p->radial_[r].angleDelta_ * 0.1f;
}

const std::vector<uint8_t>& RadialDataPacket::level(uint16_t r) const
{
   return p->radial_[r].level_;
}

size_t RadialDataPacket::data_size() const
{
   return p->dataSize_;
}

bool RadialDataPacket::Parse(std::istream& is)
{
   bool   blockValid = true;
   size_t bytesRead  = 0;

   is.read(reinterpret_cast<char*>(&p->packetCode_), 2);
   is.read(reinterpret_cast<char*>(&p->indexOfFirstRangeBin_), 2);
   is.read(reinterpret_cast<char*>(&p->numberOfRangeBins_), 2);
   is.read(reinterpret_cast<char*>(&p->iCenterOfSweep_), 2);
   is.read(reinterpret_cast<char*>(&p->jCenterOfSweep_), 2);
   is.read(reinterpret_cast<char*>(&p->scaleFactor_), 2);
   is.read(reinterpret_cast<char*>(&p->numberOfRadials_), 2);
   bytesRead += 14;

   p->packetCode_           = ntohs(p->packetCode_);
   p->indexOfFirstRangeBin_ = ntohs(p->indexOfFirstRangeBin_);
   p->numberOfRangeBins_    = ntohs(p->numberOfRangeBins_);
   p->iCenterOfSweep_       = ntohs(p->iCenterOfSweep_);
   p->jCenterOfSweep_       = ntohs(p->jCenterOfSweep_);
   p->scaleFactor_          = ntohs(p->scaleFactor_);
   p->numberOfRadials_      = ntohs(p->numberOfRadials_);

   if (is.eof())
   {
      BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Reached end of file";
      blockValid = false;
   }
   else
   {
      if (p->packetCode_ != 0xAF1F)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid packet code: " << p->packetCode_;
         blockValid = false;
      }
      if (p->numberOfRangeBins_ < 1 || p->numberOfRangeBins_ > 460)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_
            << "Invalid number of range bins: " << p->numberOfRangeBins_;
         blockValid = false;
      }
      if (p->numberOfRadials_ < 1 || p->numberOfRadials_ > 400)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_
            << "Invalid number of radials: " << p->numberOfRadials_;
         blockValid = false;
      }
   }

   if (blockValid)
   {
      p->radial_.resize(p->numberOfRadials_);

      for (uint16_t r = 0; r < p->numberOfRadials_; r++)
      {
         auto& radial = p->radial_[r];

         is.read(reinterpret_cast<char*>(&radial.numberOfRleHalfwords_), 2);
         is.read(reinterpret_cast<char*>(&radial.startAngle_), 2);
         is.read(reinterpret_cast<char*>(&radial.angleDelta_), 2);
         bytesRead += 6;

         radial.numberOfRleHalfwords_ = ntohs(radial.numberOfRleHalfwords_);
         radial.startAngle_           = ntohs(radial.startAngle_);
         radial.angleDelta_           = ntohs(radial.angleDelta_);

         if (radial.numberOfRleHalfwords_ < 1 ||
             radial.numberOfRleHalfwords_ > 230)
         {
            BOOST_LOG_TRIVIAL(warning)
               << logPrefix_ << "Invalid number of RLE halfwords: "
               << radial.numberOfRleHalfwords_ << " (Radial " << r << ")";
            blockValid = false;
            break;
         }

         // Read RLE halfwords
         size_t dataSize = radial.numberOfRleHalfwords_ * 2;
         radial.data_.resize(dataSize);
         is.read(reinterpret_cast<char*>(radial.data_.data()), dataSize);
         bytesRead += dataSize;

         // If the final byte is 0, truncate it
         if (radial.data_.back() == 0)
         {
            radial.data_.pop_back();
         }

         radial.level_.resize(radial.data_.size());

         std::transform(std::execution::par_unseq,
                        radial.data_.cbegin(),
                        radial.data_.cend(),
                        radial.level_.begin(),
                        [](uint8_t data) -> uint8_t { return data & 0x0f; });
      }
   }

   p->dataSize_ = bytesRead;

   if (!ValidateMessage(is, bytesRead))
   {
      blockValid = false;
   }

   return blockValid;
}

std::shared_ptr<RadialDataPacket> RadialDataPacket::Create(std::istream& is)
{
   std::shared_ptr<RadialDataPacket> packet =
      std::make_shared<RadialDataPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
