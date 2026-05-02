#include <scwx/grib2/grib2_message.hpp>
#include <scwx/util/logger.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace scwx::grib2
{

static const std::string logPrefix_ = "scwx::grib2::grib2_message";
static const auto        logger_    = util::Logger::Create(logPrefix_);

// Big-endian read helpers
static inline uint16_t ReadU16(const uint8_t* p)
{
   return static_cast<uint16_t>(p[0]) << 8 | static_cast<uint16_t>(p[1]);
}
static inline uint32_t ReadU32(const uint8_t* p)
{
   return static_cast<uint32_t>(p[0]) << 24 |
          static_cast<uint32_t>(p[1]) << 16 | static_cast<uint32_t>(p[2]) << 8 |
          static_cast<uint32_t>(p[3]);
}
static inline uint64_t ReadU64(const uint8_t* p)
{
   return static_cast<uint64_t>(p[0]) << 56 |
          static_cast<uint64_t>(p[1]) << 48 |
          static_cast<uint64_t>(p[2]) << 40 |
          static_cast<uint64_t>(p[3]) << 32 |
          static_cast<uint64_t>(p[4]) << 24 |
          static_cast<uint64_t>(p[5]) << 16 | static_cast<uint64_t>(p[6]) << 8 |
          static_cast<uint64_t>(p[7]);
}
static inline int16_t ReadS16(const uint8_t* p)
{
   return static_cast<int16_t>(ReadU16(p));
}

// IEEE 754 float from 4 big-endian bytes
static float ReadFloat32(const uint8_t* p)
{
   uint32_t bits = ReadU32(p);
   float    val;
   memcpy(&val, &bits, sizeof(val));
   return val;
}

// Find start of a GRIB2 section in the message
static const uint8_t*
FindSection(const uint8_t* start, size_t totalLen, uint8_t sectionNum)
{
   const uint8_t* p   = start;
   const uint8_t* end = start + totalLen;

   // Skip Section 0 (Indicator) — 16 bytes for edition 2
   p += 16;

   while (p + 5 <= end)
   {
      uint32_t secLen = ReadU32(p);
      uint8_t  secNum = p[4];

      if (secNum == sectionNum)
      {
         return p;
      }

      if (secLen < 5 || p + secLen > end)
      {
         break;
      }
      p += secLen;
   }
   return nullptr;
}

class Grib2Message::Impl
{
public:
   explicit Impl(const std::vector<char>& data) : data_(data), isValid_(false)
   {
      if (data_.size() < 16)
      {
         return;
      }

      const uint8_t* raw = reinterpret_cast<const uint8_t*>(data_.data());

      // Section 0: Indicator
      if (memcmp(raw, "GRIB", 4) != 0)
      {
         logger_->warn("Not a GRIB file (missing GRIB magic)");
         return;
      }

      if (raw[7] != 2)
      {
         logger_->warn("Unsupported GRIB edition: {}", raw[7]);
         return;
      }

      totalLen_ = ReadU64(raw + 8);
      if (totalLen_ < 16 || totalLen_ > data_.size())
      {
         logger_->warn("Invalid total length: {}", totalLen_);
         return;
      }

      discipline_ = static_cast<Discipline>(raw[6]);

      // Section 1: Identification
      ParseSection1(raw);
      // Section 3: Grid Definition
      ParseSection3(raw);
      // Section 4: Product Definition
      ParseSection4(raw);
      // Section 5: Data Representation
      ParseSection5(raw);
      // Section 6: Bit-map (optional — GFS usually doesn't use one)
      ParseSection6(raw);
      // Section 7: Data
      ParseSection7(raw);

      if (gridDef_.has_value() && simplePacking_.has_value())
      {
         isValid_ = true;
      }
   }

   ~Impl() = default;

   void ParseSection1(const uint8_t* raw)
   {
      const uint8_t* sec = FindSection(raw, totalLen_, 1);
      if (!sec)
      {
         return;
      }

      uint32_t len = ReadU32(sec);
      if (len < 21)
      {
         return;
      }

      // Bytes: center(2), subcenter(2), masterTable(1), localTable(1),
      //        sigRefTime(1), year(2), month(1), day(1), hour(1), minute(1),
      //        second(1)
      // (Not stored currently — available for future use)
   }

   void ParseSection3(const uint8_t* raw)
   {
      const uint8_t* sec = FindSection(raw, totalLen_, 3);
      if (!sec)
      {
         return;
      }

      uint32_t len = ReadU32(sec);
      if (len < 72)
      {
         return;
      }

      // sourceOfGridDefinition(1) | numDataPoints(4) |
      // numOctetsList(1) | interpList(1) | templateNum(2)
      uint16_t templateNum = ReadU16(sec + 9);

      if (templateNum != 0)
      {
         logger_->debug("Unsupported grid template: {}", templateNum);
         return;
      }

      // Grid Template 3.0: Latitude/Longitude
      // Ni(4) | Nj(4) | latFirst(4) | lonFirst(4) | resFlags(1) |
      // latLast(4) | lonLast(4) | Di(4) | Dj(4) | scanningMode(1)
      const uint8_t* gt = sec + 11;

      GridDef gd {};
      gd.ni_       = ReadU32(gt + 0);
      gd.nj_       = ReadU32(gt + 4);
      gd.latFirst_ = ReadU32(gt + 8) / 1e6;
      gd.lonFirst_ = ReadU32(gt + 12) / 1e6;
      // resFlags at gt+16
      gd.latLast_      = ReadU32(gt + 17) / 1e6;
      gd.lonLast_      = ReadU32(gt + 21) / 1e6;
      gd.di_           = ReadU32(gt + 25) / 1e6;
      gd.dj_           = ReadU32(gt + 29) / 1e6;
      gd.scanningMode_ = gt[33];

      gridDef_ = gd;
   }

   void ParseSection4(const uint8_t* raw)
   {
      const uint8_t* sec = FindSection(raw, totalLen_, 4);
      if (!sec)
      {
         return;
      }

      uint32_t len = ReadU32(sec);
      if (len < 9)
      {
         return;
      }

      // numCoordValues(2) | templateNum(2)
      uint16_t templateNum = ReadU16(sec + 7);

      if (templateNum != 0)
      {
         logger_->debug("Unsupported product template: {}", templateNum);
         return;
      }

      // Template 4.0: Analysis or Forecast
      // paramCat(1) | paramNum(1) | genProcType(1) | bgProcID(1) |
      // genProcID(1) | hoursAfter(2) | minutesAfter(1) | unitTimeRange(1) |
      // forecastTime(4) | surfType(1) | surfScale(1) | surfValue(4)
      const uint8_t* pt = sec + 9;

      parameter_.discipline_ = static_cast<uint8_t>(discipline_);
      parameter_.category_   = pt[0];
      parameter_.number_     = pt[1];
      parameter_.name_       = ParameterName(parameter_);

      surfaceType_ = static_cast<SurfaceType>(pt[12]);

      // Scaled value of first fixed surface
      uint8_t surfScale = pt[13];
      int32_t surfRaw   = static_cast<int32_t>(ReadU32(pt + 14));
      if (surfScale == 0)
      {
         levelValue_ = static_cast<double>(surfRaw);
      }
      else
      {
         levelValue_ =
            surfRaw * std::pow(10.0, -static_cast<double>(surfScale));
      }

      // Forecast time
      // unitOfTimeRange + forecastTime are position-dependent on template
      // For template 4.0: unit at offset 9+9, forecastTime at 9+10
      uint8_t timeUnit    = pt[9];
      int32_t forecastRaw = static_cast<int32_t>(ReadU32(pt + 10));

      // Convert to hours
      switch (timeUnit)
      {
      case 0: // minutes
         forecastHour_ = forecastRaw / 60.0;
         break;
      case 1: // hours
         forecastHour_ = forecastRaw;
         break;
      case 2: // days
         forecastHour_ = forecastRaw * 24.0;
         break;
      default:
         forecastHour_ = forecastRaw;
      }
   }

   void ParseSection5(const uint8_t* raw)
   {
      const uint8_t* sec = FindSection(raw, totalLen_, 5);
      if (!sec)
      {
         return;
      }

      uint32_t len = ReadU32(sec);
      if (len < 11)
      {
         return;
      }

      // numDataPoints(4) | templateNum(2)
      uint16_t templateNum = ReadU16(sec + 7);

      if (templateNum != 0)
      {
         logger_->debug("Unsupported data rep template: {}", templateNum);
         return;
      }

      // Template 5.0: Simple Packing
      // refValue(4) | binScale(2) | decScale(2) | bitsPerValue(1) |
      // typeField(1)
      const uint8_t* dt = sec + 9;

      SimplePacking sp {};
      sp.referenceValue_     = ReadFloat32(dt + 0);
      sp.binaryScaleFactor_  = ReadS16(dt + 4);
      sp.decimalScaleFactor_ = ReadS16(dt + 6);
      sp.bitsPerValue_       = dt[8];

      simplePacking_ = sp;
   }

   void ParseSection6(const uint8_t* raw)
   {
      const uint8_t* sec = FindSection(raw, totalLen_, 6);
      if (!sec)
      {
         return;
      }

      uint32_t len = ReadU32(sec);
      if (len < 6)
      {
         return;
      }

      hasBitmap_ = (sec[5] != 0);
   }

   void ParseSection7(const uint8_t* raw)
   {
      const uint8_t* sec = FindSection(raw, totalLen_, 7);
      if (!sec || !simplePacking_.has_value() || !gridDef_.has_value())
      {
         return;
      }

      uint32_t len           = ReadU32(sec);
      uint32_t numDataPts    = gridDef_->ni_ * gridDef_->nj_;
      uint32_t expectedBits  = numDataPts * simplePacking_->bitsPerValue_;
      uint32_t availableBits = (len - 5) * 8;

      if (expectedBits > availableBits)
      {
         // Bitmap may reduce data points
         if (!hasBitmap_)
         {
            logger_->warn("Data section too small: need {} bits, have {} bits",
                          expectedBits,
                          availableBits);
            return;
         }
      }

      const uint8_t* dataStart = sec + 5;
      const auto&    sp        = *simplePacking_;

      values_.reserve(numDataPts);
      uint64_t bitPos = 0;

      for (uint32_t i = 0; i < numDataPts; ++i)
      {
         // Read unsigned integer value from packed bits (MSB first, big-endian)
         uint64_t rawVal = 0;
         for (uint8_t b = 0; b < sp.bitsPerValue_; ++b)
         {
            size_t byteIdx = (bitPos + b) / 8;
            size_t bitIdx  = 7 - ((bitPos + b) % 8);
            if (byteIdx >= len - 5)
            {
               break;
            }
            rawVal = (rawVal << 1) | ((dataStart[byteIdx] >> bitIdx) & 1);
         }
         bitPos += sp.bitsPerValue_;

         // Simple packing decode:
         // float = (raw * 2^binScale + refValue) * 10^(-decScale)
         double value =
            (static_cast<double>(rawVal) *
                std::ldexp(1.0, sp.binaryScaleFactor_) +
             static_cast<double>(sp.referenceValue_)) *
            std::pow(10.0, -static_cast<double>(sp.decimalScaleFactor_));

         values_.push_back(value);
      }
   }

   static std::string ParameterName(const ParameterId& param)
   {
      if (param.discipline_ == 0)
      {
         if (param.category_ == 0 && param.number_ == 0)
            return "TMP";
         if (param.category_ == 1 && param.number_ == 1)
            return "RH";
         if (param.category_ == 2 && param.number_ == 2)
            return "UGRD";
         if (param.category_ == 2 && param.number_ == 3)
            return "VGRD";
         if (param.category_ == 3 && param.number_ == 5)
            return "HGT";
      }
      char buf[64];
      snprintf(buf,
               sizeof(buf),
               "Unknown(%d,%d,%d)",
               param.discipline_,
               param.category_,
               param.number_);
      return std::string(buf);
   }

   std::vector<char>            data_;
   bool                         isValid_ {false};
   size_t                       totalLen_ {0};
   Discipline                   discipline_ {Discipline::Unknown};
   ParameterId                  parameter_ {};
   SurfaceType                  surfaceType_ {SurfaceType::Unknown};
   double                       levelValue_ {0.0};
   double                       forecastHour_ {0.0};
   std::optional<GridDef>       gridDef_ {};
   std::optional<SimplePacking> simplePacking_ {};
   bool                         hasBitmap_ {false};
   std::vector<double>          values_ {};
};

Grib2Message::Grib2Message(const std::vector<char>& data) :
    p(std::make_unique<Impl>(data))
{
}
Grib2Message::~Grib2Message() = default;

Grib2Message::Grib2Message(Grib2Message&&) noexcept            = default;
Grib2Message& Grib2Message::operator=(Grib2Message&&) noexcept = default;

bool Grib2Message::IsValid() const
{
   return p->isValid_;
}
Discipline Grib2Message::discipline() const
{
   return p->discipline_;
}
ParameterId Grib2Message::parameter() const
{
   return p->parameter_;
}
SurfaceType Grib2Message::surfaceType() const
{
   return p->surfaceType_;
}
double Grib2Message::levelValue() const
{
   return p->levelValue_;
}
double Grib2Message::forecastHour() const
{
   return p->forecastHour_;
}

std::optional<GridDef> Grib2Message::gridDefinition() const
{
   return p->gridDef_;
}
std::optional<SimplePacking> Grib2Message::simplePacking() const
{
   return p->simplePacking_;
}

std::vector<double> Grib2Message::values() const
{
   return p->values_;
}

double Grib2Message::valueAt(std::size_t i, std::size_t j) const
{
   if (!p->gridDef_.has_value() || p->values_.empty())
   {
      return 0.0;
   }

   const auto& gd = p->gridDef_.value();
   if (i >= gd.ni_ || j >= gd.nj_)
   {
      return 0.0;
   }

   bool scanSouthToNorth = (gd.scanningMode_ & 0x80) != 0;
   bool scanEastToWest   = (gd.scanningMode_ & 0x40) != 0;

   size_t index;
   if (scanSouthToNorth)
   {
      index = j * gd.ni_ + (scanEastToWest ? (gd.ni_ - 1 - i) : i);
   }
   else
   {
      index =
         (gd.nj_ - 1 - j) * gd.ni_ + (scanEastToWest ? (gd.ni_ - 1 - i) : i);
   }

   return (index < p->values_.size()) ? p->values_[index] : 0.0;
}

std::size_t Grib2Message::dataPointCount() const
{
   return p->values_.size();
}

const std::string& Grib2Message::errorMessage() const
{
   static const std::string ok;
   return ok;
}

} // namespace scwx::grib2
