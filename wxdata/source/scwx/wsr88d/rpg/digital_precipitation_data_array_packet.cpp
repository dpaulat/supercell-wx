#include <scwx/wsr88d/rpg/digital_precipitation_data_array_packet.hpp>

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
   "[scwx::wsr88d::rpg::digital_precipitation_data_array_packet] ";

class DigitalPrecipitationDataArrayPacketImpl
{
public:
   struct Row
   {
      uint16_t             numberOfBytes_;
      std::vector<uint8_t> run_;
      std::vector<uint8_t> level_;

      Row() : numberOfBytes_ {0}, run_ {}, level_ {} {}
   };

   explicit DigitalPrecipitationDataArrayPacketImpl() :
       packetCode_ {0},
       numberOfLfmBoxesInRow_ {0},
       numberOfRows_ {0},
       row_ {},
       dataSize_ {0}
   {
   }
   ~DigitalPrecipitationDataArrayPacketImpl() = default;

   uint16_t packetCode_;
   uint16_t numberOfLfmBoxesInRow_;
   uint16_t numberOfRows_;

   // Repeat for each row
   std::vector<Row> row_;

   size_t dataSize_;
};

DigitalPrecipitationDataArrayPacket::DigitalPrecipitationDataArrayPacket() :
    p(std::make_unique<DigitalPrecipitationDataArrayPacketImpl>())
{
}
DigitalPrecipitationDataArrayPacket::~DigitalPrecipitationDataArrayPacket() =
   default;

DigitalPrecipitationDataArrayPacket::DigitalPrecipitationDataArrayPacket(
   DigitalPrecipitationDataArrayPacket&&) noexcept = default;
DigitalPrecipitationDataArrayPacket&
DigitalPrecipitationDataArrayPacket::operator      =(
   DigitalPrecipitationDataArrayPacket&&) noexcept = default;

uint16_t DigitalPrecipitationDataArrayPacket::packet_code() const
{
   return p->packetCode_;
}

uint16_t DigitalPrecipitationDataArrayPacket::number_of_lfm_boxes_in_row() const
{
   return p->numberOfLfmBoxesInRow_;
}

uint16_t DigitalPrecipitationDataArrayPacket::number_of_rows() const
{
   return p->numberOfRows_;
}

size_t DigitalPrecipitationDataArrayPacket::data_size() const
{
   return p->dataSize_;
}

bool DigitalPrecipitationDataArrayPacket::Parse(std::istream& is)
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
      if (p->packetCode_ != 17)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid packet code: " << p->packetCode_;
         blockValid = false;
      }
      if (p->numberOfRows_ != 131)
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

         if (row.numberOfBytes_ < 2 || row.numberOfBytes_ > 262 ||
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
         size_t recordCount = row.numberOfBytes_ / 2;
         row.run_.resize(recordCount);
         row.level_.resize(recordCount);

         for (size_t i = 0; i < recordCount; i++)
         {
            is.read(reinterpret_cast<char*>(&row.run_[i]), 1);
            is.read(reinterpret_cast<char*>(&row.level_[i]), 1);
         }

         bytesRead += row.numberOfBytes_;
      }
   }

   p->dataSize_ = bytesRead;

   if (!ValidateMessage(is, bytesRead))
   {
      blockValid = false;
   }

   return blockValid;
}

std::shared_ptr<DigitalPrecipitationDataArrayPacket>
DigitalPrecipitationDataArrayPacket::Create(std::istream& is)
{
   std::shared_ptr<DigitalPrecipitationDataArrayPacket> packet =
      std::make_shared<DigitalPrecipitationDataArrayPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
