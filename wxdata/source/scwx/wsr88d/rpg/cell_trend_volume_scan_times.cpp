#include <scwx/wsr88d/rpg/cell_trend_volume_scan_times.hpp>
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
   "scwx::wsr88d::rpg::cell_trend_volume_scan_times";
static const auto logger_ = util::Logger::Create(logPrefix_);

class CellTrendVolumeScanTimesImpl
{
public:
   explicit CellTrendVolumeScanTimesImpl() :
       packetCode_ {0},
       lengthOfBlock_ {0},
       numberOfVolumes_ {0},
       latestVolumePointer_ {0},
       volumeTime_ {}
   {
   }
   ~CellTrendVolumeScanTimesImpl() = default;

   uint16_t packetCode_;
   uint16_t lengthOfBlock_;
   uint16_t numberOfVolumes_;
   uint16_t latestVolumePointer_;

   std::vector<uint16_t> volumeTime_;
};

CellTrendVolumeScanTimes::CellTrendVolumeScanTimes() :
    p(std::make_unique<CellTrendVolumeScanTimesImpl>())
{
}
CellTrendVolumeScanTimes::~CellTrendVolumeScanTimes() = default;

CellTrendVolumeScanTimes::CellTrendVolumeScanTimes(
   CellTrendVolumeScanTimes&&) noexcept                     = default;
CellTrendVolumeScanTimes& CellTrendVolumeScanTimes::operator=(
   CellTrendVolumeScanTimes&&) noexcept = default;

uint16_t CellTrendVolumeScanTimes::packet_code() const
{
   return p->packetCode_;
}

uint16_t CellTrendVolumeScanTimes::length_of_block() const
{
   return p->lengthOfBlock_;
}

uint16_t CellTrendVolumeScanTimes::number_of_volumes() const
{
   return p->numberOfVolumes_;
}

uint16_t CellTrendVolumeScanTimes::latest_volume_pointer() const
{
   return p->latestVolumePointer_;
}

uint16_t CellTrendVolumeScanTimes::volume_time(uint16_t v) const
{
   return p->volumeTime_[v];
}

size_t CellTrendVolumeScanTimes::data_size() const
{
   return p->lengthOfBlock_ + 4u;
}

bool CellTrendVolumeScanTimes::Parse(std::istream& is)
{
   bool blockValid = true;

   std::streampos isBegin = is.tellg();

   is.read(reinterpret_cast<char*>(&p->packetCode_), 2);
   is.read(reinterpret_cast<char*>(&p->lengthOfBlock_), 2);
   is.read(reinterpret_cast<char*>(&p->numberOfVolumes_), 2);

   p->packetCode_      = ntohs(p->packetCode_);
   p->lengthOfBlock_   = ntohs(p->lengthOfBlock_);
   p->numberOfVolumes_ = ntohs(p->numberOfVolumes_);

   if (is.eof())
   {
      logger_->debug("Reached end of file");
      blockValid = false;
   }
   else
   {
      if (p->packetCode_ != 22)
      {
         logger_->warn("Invalid packet code: {}", p->packetCode_);
         blockValid = false;
      }
      else if (p->lengthOfBlock_ < 4 || p->lengthOfBlock_ > 22)
      {
         logger_->warn("Invalid length of block: {}", p->packetCode_);
         blockValid = false;
      }
   }

   if (blockValid)
   {
      p->volumeTime_.resize(p->numberOfVolumes_);

      is.read(reinterpret_cast<char*>(p->volumeTime_.data()),
              p->numberOfVolumes_ * 2u);

      SwapVector(p->volumeTime_);
   }

   std::streampos isEnd = is.tellg();

   if (!ValidateMessage(is, isEnd - isBegin))
   {
      blockValid = false;
   }

   return blockValid;
}

std::shared_ptr<CellTrendVolumeScanTimes>
CellTrendVolumeScanTimes::Create(std::istream& is)
{
   std::shared_ptr<CellTrendVolumeScanTimes> packet =
      std::make_shared<CellTrendVolumeScanTimes>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
