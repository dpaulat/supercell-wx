#include <scwx/wsr88d/rpg/precipitation_rate_data_array_packet.hpp>

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
   "[scwx::wsr88d::rpg::precipitation_rate_data_array_packet] ";

class PrecipitationRateDataArrayPacketImpl
{
public:
   struct Row
   {
      uint16_t             numberOfBytes_;
      std::vector<uint8_t> data_;

      Row() : numberOfBytes_ {0}, data_ {} {}
   };

   explicit PrecipitationRateDataArrayPacketImpl() :
       packetCode_ {0},
       numberOfLfmBoxesInRow_ {0},
       numberOfRows_ {0},
       row_ {},
       dataSize_ {0} {};
   ~PrecipitationRateDataArrayPacketImpl() = default;

   uint16_t packetCode_;
   uint16_t numberOfLfmBoxesInRow_;
   uint16_t numberOfRows_;

   // Repeat for each row
   std::vector<Row> row_;

   size_t dataSize_;
};

PrecipitationRateDataArrayPacket::PrecipitationRateDataArrayPacket() :
    p(std::make_unique<PrecipitationRateDataArrayPacketImpl>())
{
}
PrecipitationRateDataArrayPacket::~PrecipitationRateDataArrayPacket() = default;

PrecipitationRateDataArrayPacket::PrecipitationRateDataArrayPacket(
   PrecipitationRateDataArrayPacket&&) noexcept = default;
PrecipitationRateDataArrayPacket& PrecipitationRateDataArrayPacket::operator=(
   PrecipitationRateDataArrayPacket&&) noexcept = default;

uint16_t PrecipitationRateDataArrayPacket::packet_code() const
{
   return p->packetCode_;
}

uint16_t PrecipitationRateDataArrayPacket::number_of_lfm_boxes_in_row() const
{
   return p->numberOfLfmBoxesInRow_;
}

uint16_t PrecipitationRateDataArrayPacket::number_of_rows() const
{
   return p->numberOfRows_;
}

size_t PrecipitationRateDataArrayPacket::data_size() const
{
   return p->dataSize_;
}

bool PrecipitationRateDataArrayPacket::Parse(std::istream& is)
{
   bool   blockValid = true;
   size_t bytesRead  = 0;

   is.read(reinterpret_cast<char*>(&p->packetCode_), 2);
   is.seekg(4, std::ios_base::cur);
   is.read(reinterpret_cast<char*>(&p->numberOfLfmBoxesInRow_), 2);
   is.read(reinterpret_cast<char*>(&p->numberOfRows_), 2);
   bytesRead += 10;

   p->packetCode_            = ntohs(p->packetCode_);
   p->numberOfLfmBoxesInRow_ = ntohs(p->numberOfLfmBoxesInRow_);
   p->numberOfRows_          = ntohs(p->numberOfRows_);

   if (is.eof())
   {
      BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Reached end of file";
      blockValid = false;
   }
   else
   {
      if (p->packetCode_ != 18)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid packet code: " << p->packetCode_;
         blockValid = false;
      }
      if (p->numberOfRows_ != 13)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid number of rows: " << p->numberOfRows_;
         blockValid = false;
      }
   }

   if (blockValid)
   {
      p->row_.resize(p->numberOfRows_);

      for (uint16_t r = 0; r < p->numberOfRows_; r++)
      {
         size_t rowBytesRead = 0;

         auto& row = p->row_[r];

         is.read(reinterpret_cast<char*>(&row.numberOfBytes_), 2);
         bytesRead += 2;

         row.numberOfBytes_ = ntohs(row.numberOfBytes_);

         if (row.numberOfBytes_ < 2 || row.numberOfBytes_ > 14 ||
             row.numberOfBytes_ % 2 != 0)
         {
            BOOST_LOG_TRIVIAL(warning)
               << logPrefix_
               << "Invalid number of bytes in row: " << row.numberOfBytes_
               << " (Row " << r << ")";
            blockValid = false;
            break;
         }

         // Read row data
         size_t dataSize = row.numberOfBytes_;
         row.data_.resize(dataSize);
         is.read(reinterpret_cast<char*>(row.data_.data()), dataSize);
         bytesRead += dataSize;

         // If the final byte is 0, truncate it
         if (row.data_.back() == 0)
         {
            row.data_.pop_back();
         }
      }
   }

   p->dataSize_ = bytesRead;

   if (!ValidateMessage(is, bytesRead))
   {
      blockValid = false;
   }

   return blockValid;
}

std::shared_ptr<PrecipitationRateDataArrayPacket>
PrecipitationRateDataArrayPacket::Create(std::istream& is)
{
   std::shared_ptr<PrecipitationRateDataArrayPacket> packet =
      std::make_shared<PrecipitationRateDataArrayPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
