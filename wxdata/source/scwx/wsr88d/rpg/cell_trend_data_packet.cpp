#include <scwx/wsr88d/rpg/cell_trend_data_packet.hpp>
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
   "scwx::wsr88d::rpg::cell_trend_data_packet";
static const auto logger_ = util::Logger::Create(logPrefix_);

struct CellTrendData
{
   uint16_t              trendCode_;
   uint8_t               numberOfVolumes_;
   uint8_t               latestVolumePointer_;
   std::vector<uint16_t> trendData_;

   CellTrendData() :
       trendCode_ {0},
       numberOfVolumes_ {0},
       latestVolumePointer_ {0},
       trendData_ {}
   {
   }
};

class CellTrendDataPacketImpl
{
public:
   explicit CellTrendDataPacketImpl() :
       packetCode_ {0},
       lengthOfBlock_ {0},
       cellId_ {0},
       iPosition_ {0},
       jPosition_ {0},
       trendData_ {}
   {
   }
   ~CellTrendDataPacketImpl() = default;

   uint16_t    packetCode_;
   uint16_t    lengthOfBlock_;
   std::string cellId_;
   int16_t     iPosition_;
   int16_t     jPosition_;

   std::vector<CellTrendData> trendData_;
};

CellTrendDataPacket::CellTrendDataPacket() :
    p(std::make_unique<CellTrendDataPacketImpl>())
{
}
CellTrendDataPacket::~CellTrendDataPacket() = default;

CellTrendDataPacket::CellTrendDataPacket(CellTrendDataPacket&&) noexcept =
   default;
CellTrendDataPacket&
CellTrendDataPacket::operator=(CellTrendDataPacket&&) noexcept = default;

uint16_t CellTrendDataPacket::packet_code() const
{
   return p->packetCode_;
}

uint16_t CellTrendDataPacket::length_of_block() const
{
   return p->lengthOfBlock_;
}

std::string CellTrendDataPacket::cell_id() const
{
   return p->cellId_;
}

int16_t CellTrendDataPacket::i_position() const
{
   return p->iPosition_;
}

int16_t CellTrendDataPacket::j_position() const
{
   return p->iPosition_;
}

uint16_t CellTrendDataPacket::number_of_trends() const
{
   return static_cast<uint16_t>(p->trendData_.size());
}

uint16_t CellTrendDataPacket::trend_code(uint16_t t) const
{
   return p->trendData_[t].trendCode_;
}

uint8_t CellTrendDataPacket::number_of_volumes(uint16_t t) const
{
   return p->trendData_[t].numberOfVolumes_;
}

uint8_t CellTrendDataPacket::latest_volume_pointer(uint16_t t) const
{
   return p->trendData_[t].latestVolumePointer_;
}

uint16_t CellTrendDataPacket::trend_data(uint16_t t, uint8_t v) const
{
   return p->trendData_[t].trendData_[v];
}

size_t CellTrendDataPacket::data_size() const
{
   return p->lengthOfBlock_ + 4u;
}

bool CellTrendDataPacket::Parse(std::istream& is)
{
   bool blockValid = true;

   std::streampos isBegin = is.tellg();

   is.read(reinterpret_cast<char*>(&p->packetCode_), 2);
   is.read(reinterpret_cast<char*>(&p->lengthOfBlock_), 2);

   p->packetCode_    = ntohs(p->packetCode_);
   p->lengthOfBlock_ = ntohs(p->lengthOfBlock_);

   if (is.eof())
   {
      logger_->debug("Reached end of file");
      blockValid = false;
   }
   else
   {
      if (p->packetCode_ != 21)
      {
         logger_->warn("Invalid packet code: {}", p->packetCode_);
         blockValid = false;
      }
      else if (p->lengthOfBlock_ < 12 || p->lengthOfBlock_ > 198)
      {
         logger_->warn("Invalid length of block: {}", p->packetCode_);
         blockValid = false;
      }
   }

   p->cellId_.resize(2);

   is.read(p->cellId_.data(), 2);
   is.read(reinterpret_cast<char*>(&p->iPosition_), 2);
   is.read(reinterpret_cast<char*>(&p->jPosition_), 2);

   p->iPosition_ = ntohs(p->iPosition_);
   p->jPosition_ = ntohs(p->jPosition_);

   if (blockValid)
   {
      size_t trendSize = p->lengthOfBlock_ - 6;
      size_t bytesRead = 0;

      while (bytesRead < trendSize)
      {
         CellTrendData trendData;

         is.read(reinterpret_cast<char*>(&trendData.trendCode_), 2);
         is.read(reinterpret_cast<char*>(&trendData.numberOfVolumes_), 1);
         is.read(reinterpret_cast<char*>(&trendData.latestVolumePointer_), 1);

         trendData.trendCode_ = ntohs(trendData.trendCode_);

         size_t trendDataSize =
            static_cast<size_t>(trendData.numberOfVolumes_) * 2u;
         trendData.trendData_.resize(trendData.numberOfVolumes_);

         is.read(reinterpret_cast<char*>(trendData.trendData_.data()),
                 trendDataSize);

         SwapVector(trendData.trendData_);

         p->trendData_.push_back(std::move(trendData));

         bytesRead += 4 + trendDataSize;
      }
   }

   std::streampos isEnd = is.tellg();

   if (!ValidateMessage(is, isEnd - isBegin))
   {
      blockValid = false;
   }

   return blockValid;
}

std::shared_ptr<CellTrendDataPacket>
CellTrendDataPacket::Create(std::istream& is)
{
   std::shared_ptr<CellTrendDataPacket> packet =
      std::make_shared<CellTrendDataPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
