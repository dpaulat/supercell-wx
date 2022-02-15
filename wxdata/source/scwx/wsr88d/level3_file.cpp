#include <scwx/wsr88d/level3_file.hpp>
#include <scwx/wsr88d/rpg/ccb_header.hpp>
#include <scwx/wsr88d/rpg/level3_message_factory.hpp>

#include <fstream>

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{

static const std::string logPrefix_ = "[scwx::wsr88d::level3_file] ";

class Level3FileImpl
{
public:
   explicit Level3FileImpl() :
       wmoHeader_ {}, ccbHeader_ {}, innerHeader_ {}, message_ {} {};
   ~Level3FileImpl() = default;

   bool DecompressFile(std::istream& is, std::stringstream& ss);
   bool LoadFileData(std::istream& is);

   std::shared_ptr<awips::WmoHeader>   wmoHeader_;
   std::shared_ptr<rpg::CcbHeader>     ccbHeader_;
   std::shared_ptr<awips::WmoHeader>   innerHeader_;
   std::shared_ptr<rpg::Level3Message> message_;
};

Level3File::Level3File() : p(std::make_unique<Level3FileImpl>()) {}
Level3File::~Level3File() = default;

Level3File::Level3File(Level3File&&) noexcept = default;
Level3File& Level3File::operator=(Level3File&&) noexcept = default;

std::shared_ptr<awips::WmoHeader> Level3File::wmo_header() const
{
   return p->wmoHeader_;
}

std::shared_ptr<rpg::Level3Message> Level3File::message() const
{
   return p->message_;
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

   p->wmoHeader_ = std::make_shared<awips::WmoHeader>();

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
      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << "Site ID:   " << p->wmoHeader_->product_designator();

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
      innerHeader_ = std::make_shared<awips::WmoHeader>();
      dataValid    = innerHeader_->Parse(ss);
   }

   return dataValid;
}

bool Level3FileImpl::LoadFileData(std::istream& is)
{
   message_ = rpg::Level3MessageFactory::Create(is);

   return (message_ != nullptr);
}

} // namespace wsr88d
} // namespace scwx
