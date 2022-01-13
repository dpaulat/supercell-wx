#include <scwx/wsr88d/level3_file.hpp>
#include <scwx/wsr88d/rpg/ccb_header.hpp>
#include <scwx/wsr88d/rpg/graphic_alphanumeric_block.hpp>
#include <scwx/wsr88d/rpg/level3_message_header.hpp>
#include <scwx/wsr88d/rpg/product_description_block.hpp>
#include <scwx/wsr88d/rpg/product_symbology_block.hpp>
#include <scwx/wsr88d/rpg/tabular_alphanumeric_block.hpp>
#include <scwx/wsr88d/rpg/wmo_header.hpp>
#include <scwx/util/rangebuf.hpp>
#include <scwx/util/time.hpp>

#include <fstream>
#include <set>

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{

static const std::string logPrefix_ = "[scwx::wsr88d::level3_file] ";

static const std::set<int16_t> standaloneTabularProducts_ = {62, 75, 77, 82};

class Level3FileImpl
{
public:
   explicit Level3FileImpl() :
       wmoHeader_ {},
       ccbHeader_ {},
       messageHeader_ {},
       descriptionBlock_ {},
       symbologyBlock_ {},
       graphicBlock_ {},
       tabularBlock_ {} {};
   ~Level3FileImpl() = default;

   bool DecompressFile(std::istream& is, std::stringstream& ss);
   bool LoadFileData(std::istream& is);
   bool LoadBlocks(std::istream& is);

   std::shared_ptr<rpg::WmoHeader>                wmoHeader_;
   std::shared_ptr<rpg::CcbHeader>                ccbHeader_;
   std::shared_ptr<rpg::WmoHeader>                innerHeader_;
   std::shared_ptr<rpg::Level3MessageHeader>      messageHeader_;
   std::shared_ptr<rpg::ProductDescriptionBlock>  descriptionBlock_;
   std::shared_ptr<rpg::ProductSymbologyBlock>    symbologyBlock_;
   std::shared_ptr<rpg::GraphicAlphanumericBlock> graphicBlock_;
   std::shared_ptr<rpg::TabularAlphanumericBlock> tabularBlock_;
};

Level3File::Level3File() : p(std::make_unique<Level3FileImpl>()) {}
Level3File::~Level3File() = default;

Level3File::Level3File(Level3File&&) noexcept = default;
Level3File& Level3File::operator=(Level3File&&) noexcept = default;

std::shared_ptr<rpg::Level3MessageHeader> Level3File::message_header() const
{
   return p->messageHeader_;
}

std::shared_ptr<rpg::ProductSymbologyBlock>
Level3File::product_symbology_block() const
{
   return p->symbologyBlock_;
}

bool Level3File::LoadFile(const std::string& filename)
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "LoadFile(" << filename << ")";
   bool fileValid = true;

   std::ifstream f(filename, std::ios_base::in | std::ios_base::binary);
   if (!f.good())
   {
      BOOST_LOG_TRIVIAL(warning)
         << logPrefix_ << "Could not open file for reading: " << filename;
      fileValid = false;
   }

   if (fileValid)
   {
      fileValid = LoadData(f);
   }

   return fileValid;
}

bool Level3File::LoadData(std::istream& is)
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Loading Data";

   p->wmoHeader_ = std::make_shared<rpg::WmoHeader>();

   bool dataValid = p->wmoHeader_->Parse(is);

   if (dataValid)
   {
      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << "Data Type: " << p->wmoHeader_->data_type();
      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << "ICAO:      " << p->wmoHeader_->icao();
      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << "Date/Time: " << p->wmoHeader_->date_time();
      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << "Category:  " << p->wmoHeader_->product_category();

      // If the header is compressed
      if (is.peek() == 0x78)
      {
         std::stringstream ss;

         dataValid = p->DecompressFile(is, ss);

         if (dataValid)
         {
            dataValid = p->LoadFileData(ss);
         }
      }
      else
      {
         dataValid = p->LoadFileData(is);
      }
   }

   return dataValid;
}

bool Level3FileImpl::DecompressFile(std::istream& is, std::stringstream& ss)
{
   bool dataValid = true;

   std::streampos  dataStart          = is.tellg();
   std::streamsize totalBytesCopied   = 0;
   int             totalBytesConsumed = 0;

   while (dataValid && is.peek() == 0x78)
      try
      {
         boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
         boost::iostreams::zlib_decompressor zlibDecompressor;
         in.push(zlibDecompressor);
         in.push(is);

         std::streamsize bytesCopied   = boost::iostreams::copy(in, ss);
         int             bytesConsumed = zlibDecompressor.filter().total_in();

         totalBytesCopied += bytesCopied;
         totalBytesConsumed += bytesConsumed;

         is.seekg(dataStart + static_cast<std::streamoff>(totalBytesConsumed),
                  std::ios_base::beg);

         if (bytesConsumed <= 0)
         {
            // Not sure this will ever occur, but will prevent an infinite loop
            break;
         }
      }
      catch (const boost::iostreams::zlib_error& ex)
      {
         int error = ex.error();

         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Error decompressing data: " << ex.what();

         dataValid = false;
      }

   if (dataValid)
   {
      BOOST_LOG_TRIVIAL(trace)
         << logPrefix_ << "Input data consumed = " << totalBytesCopied
         << " bytes";
      BOOST_LOG_TRIVIAL(trace)
         << logPrefix_ << "Decompressed data size = " << totalBytesConsumed
         << " bytes";

      ccbHeader_ = std::make_shared<rpg::CcbHeader>();
      dataValid  = ccbHeader_->Parse(ss);
   }

   if (dataValid)
   {
      innerHeader_ = std::make_shared<rpg::WmoHeader>();
      dataValid    = innerHeader_->Parse(ss);
   }

   return dataValid;
}

bool Level3FileImpl::LoadFileData(std::istream& is)
{
   messageHeader_ = std::make_shared<rpg::Level3MessageHeader>();

   bool dataValid = messageHeader_->Parse(is);

   if (dataValid)
   {
      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << "Code:      " << messageHeader_->message_code();

      descriptionBlock_ = std::make_shared<rpg::ProductDescriptionBlock>();

      dataValid = descriptionBlock_->Parse(is);
   }

   if (dataValid)
   {
      if (descriptionBlock_->IsCompressionEnabled())
      {
         size_t messageLength = messageHeader_->length_of_message();
         size_t prefixLength =
            rpg::Level3MessageHeader::SIZE + rpg::ProductDescriptionBlock::SIZE;
         size_t recordSize =
            (messageLength > prefixLength) ? messageLength - prefixLength : 0;

         boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
         util::rangebuf r(is.rdbuf(), recordSize);
         in.push(boost::iostreams::bzip2_decompressor());
         in.push(r);

         try
         {
            std::stringstream ss;
            std::streamsize   bytesCopied = boost::iostreams::copy(in, ss);
            BOOST_LOG_TRIVIAL(trace)
               << logPrefix_ << "Decompressed data size = " << bytesCopied
               << " bytes";

            dataValid = LoadBlocks(ss);
         }
         catch (const boost::iostreams::bzip2_error& ex)
         {
            int error = ex.error();
            BOOST_LOG_TRIVIAL(warning)
               << logPrefix_ << "Error decompressing data: " << ex.what();

            dataValid = false;
         }
      }
      else
      {
         dataValid = LoadBlocks(is);
      }
   }

   return dataValid;
}

bool Level3FileImpl::LoadBlocks(std::istream& is)
{
   bool symbologyValid = true;
   bool graphicValid   = true;
   bool tabularValid   = true;

   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Loading Blocks";

   bool skipTabularHeader = false;

   std::streampos offsetBasePos = is.tellg();

   constexpr size_t offsetBase =
      rpg::Level3MessageHeader::SIZE + rpg::ProductDescriptionBlock::SIZE;

   size_t offsetToSymbology = descriptionBlock_->offset_to_symbology() * 2u;
   size_t offsetToGraphic   = descriptionBlock_->offset_to_graphic() * 2u;
   size_t offsetToTabular   = descriptionBlock_->offset_to_tabular() * 2u;

   if (standaloneTabularProducts_.contains(messageHeader_->message_code()))
   {
      // These products are completely alphanumeric, and do not contain a
      // symbology block.
      offsetToTabular   = offsetToSymbology;
      offsetToSymbology = 0;
      offsetToGraphic   = 0;

      skipTabularHeader = true;
   }

   if (offsetToSymbology >= offsetBase)
   {
      symbologyBlock_ = std::make_shared<rpg::ProductSymbologyBlock>();

      is.seekg(offsetToSymbology - offsetBase, std::ios_base::cur);
      symbologyValid = symbologyBlock_->Parse(is);
      is.seekg(offsetBasePos, std::ios_base::beg);

      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << "Product symbology block valid: " << symbologyValid;

      if (!symbologyValid)
      {
         symbologyBlock_ = nullptr;
      }
   }

   if (offsetToGraphic >= offsetBase)
   {
      graphicBlock_ = std::make_shared<rpg::GraphicAlphanumericBlock>();

      is.seekg(offsetToGraphic - offsetBase, std::ios_base::cur);
      graphicValid = graphicBlock_->Parse(is);
      is.seekg(offsetBasePos, std::ios_base::beg);

      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << "Graphic alphanumeric block valid: " << graphicValid;

      if (!graphicValid)
      {
         graphicBlock_ = nullptr;
      }
   }

   if (offsetToTabular >= offsetBase)
   {
      tabularBlock_ = std::make_shared<rpg::TabularAlphanumericBlock>();

      is.seekg(offsetToTabular - offsetBase, std::ios_base::cur);
      tabularValid = tabularBlock_->Parse(is, skipTabularHeader);
      is.seekg(offsetBasePos, std::ios_base::beg);

      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << "Tabular alphanumeric block valid: " << tabularValid;

      if (!tabularValid)
      {
         tabularBlock_ = nullptr;
      }
   }

   return (symbologyValid && graphicValid && tabularValid);
}

} // namespace wsr88d
} // namespace scwx
