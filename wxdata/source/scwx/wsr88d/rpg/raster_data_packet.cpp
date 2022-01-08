#include <scwx/wsr88d/rpg/raster_data_packet.hpp>

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
   "[scwx::wsr88d::rpg::raster_data_packet] ";

class RasterDataPacketImpl
{
public:
   struct Row
   {
      uint16_t             numberOfBytes_;
      std::vector<uint8_t> data_;

      Row() : numberOfBytes_ {0}, data_ {} {}
   };

   explicit RasterDataPacketImpl() :
       packetCode_ {0},
       opFlag_ {0},
       iCoordinateStart_ {0},
       jCoordinateStart_ {0},
       xScaleInt_ {0},
       xScaleFractional_ {0},
       yScaleInt_ {0},
       yScaleFractional_ {0},
       numberOfRows_ {0},
       packagingDescriptor_ {0},
       row_ {},
       dataSize_ {0}
   {
   }
   ~RasterDataPacketImpl() = default;

   uint16_t                packetCode_;
   std::array<uint16_t, 2> opFlag_;
   int16_t                 iCoordinateStart_;
   int16_t                 jCoordinateStart_;
   uint16_t                xScaleInt_;
   uint16_t                xScaleFractional_;
   uint16_t                yScaleInt_;
   uint16_t                yScaleFractional_;
   uint16_t                numberOfRows_;
   uint16_t                packagingDescriptor_;

   // Repeat for each row
   std::vector<Row> row_;

   size_t dataSize_;
};

RasterDataPacket::RasterDataPacket() :
    p(std::make_unique<RasterDataPacketImpl>())
{
}
RasterDataPacket::~RasterDataPacket() = default;

RasterDataPacket::RasterDataPacket(RasterDataPacket&&) noexcept = default;
RasterDataPacket&
RasterDataPacket::operator=(RasterDataPacket&&) noexcept = default;

uint16_t RasterDataPacket::packet_code() const
{
   return p->packetCode_;
}

uint16_t RasterDataPacket::op_flag(size_t i) const
{
   return p->opFlag_[i];
}

int16_t RasterDataPacket::i_coordinate_start() const
{
   return p->iCoordinateStart_;
}

int16_t RasterDataPacket::j_coordinate_start() const
{
   return p->jCoordinateStart_;
}

uint16_t RasterDataPacket::x_scale_int() const
{
   return p->xScaleInt_;
}

uint16_t RasterDataPacket::x_scale_fractional() const
{
   return p->xScaleFractional_;
}

uint16_t RasterDataPacket::y_scale_int() const
{
   return p->yScaleInt_;
}

uint16_t RasterDataPacket::y_scale_fractional() const
{
   return p->yScaleFractional_;
}

uint16_t RasterDataPacket::number_of_rows() const
{
   return p->numberOfRows_;
}

uint16_t RasterDataPacket::packaging_descriptor() const
{
   return p->packagingDescriptor_;
}

size_t RasterDataPacket::data_size() const
{
   return p->dataSize_;
}

bool RasterDataPacket::Parse(std::istream& is)
{
   bool   blockValid = true;
   size_t bytesRead  = 0;

   is.read(reinterpret_cast<char*>(&p->packetCode_), 2);
   is.read(reinterpret_cast<char*>(p->opFlag_.data()), 4);
   is.read(reinterpret_cast<char*>(&p->iCoordinateStart_), 2);
   is.read(reinterpret_cast<char*>(&p->jCoordinateStart_), 2);
   is.read(reinterpret_cast<char*>(&p->xScaleInt_), 2);
   is.read(reinterpret_cast<char*>(&p->xScaleFractional_), 2);
   is.read(reinterpret_cast<char*>(&p->yScaleInt_), 2);
   is.read(reinterpret_cast<char*>(&p->yScaleFractional_), 2);
   is.read(reinterpret_cast<char*>(&p->numberOfRows_), 2);
   is.read(reinterpret_cast<char*>(&p->packagingDescriptor_), 2);
   bytesRead += 22;

   p->packetCode_          = ntohs(p->packetCode_);
   p->iCoordinateStart_    = ntohs(p->iCoordinateStart_);
   p->jCoordinateStart_    = ntohs(p->jCoordinateStart_);
   p->xScaleInt_           = ntohs(p->xScaleInt_);
   p->xScaleFractional_    = ntohs(p->xScaleFractional_);
   p->yScaleInt_           = ntohs(p->yScaleInt_);
   p->yScaleFractional_    = ntohs(p->yScaleFractional_);
   p->numberOfRows_        = ntohs(p->numberOfRows_);
   p->packagingDescriptor_ = ntohs(p->packagingDescriptor_);

   SwapArray(p->opFlag_);

   if (is.eof())
   {
      BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Reached end of file";
      blockValid = false;
   }
   else
   {
      if (p->packetCode_ != 0xBA0F && p->packetCode_ != 0xBA07)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid packet code: " << p->packetCode_;
         blockValid = false;
      }
      if (p->numberOfRows_ < 1 || p->numberOfRows_ > 464)
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

         if (row.numberOfBytes_ < 2 || row.numberOfBytes_ > 920 ||
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

std::shared_ptr<RasterDataPacket> RasterDataPacket::Create(std::istream& is)
{
   std::shared_ptr<RasterDataPacket> packet =
      std::make_shared<RasterDataPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
