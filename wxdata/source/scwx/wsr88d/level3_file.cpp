#include <scwx/wsr88d/level3_file.hpp>
#include <scwx/wsr88d/rpg/level3_message_header.hpp>
#include <scwx/wsr88d/rpg/product_description_block.hpp>
#include <scwx/wsr88d/rpg/wmo_header.hpp>
#include <scwx/util/rangebuf.hpp>
#include <scwx/util/time.hpp>

#include <fstream>

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
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
       wmoHeader_ {}, messageHeader_ {}, description_ {} {};
   ~Level3FileImpl() = default;

   void LoadBlocks(std::istream& is);

   rpg::WmoHeader               wmoHeader_;
   rpg::Level3MessageHeader     messageHeader_;
   rpg::ProductDescriptionBlock description_;

   size_t numRecords_;
};

Level3File::Level3File() : p(std::make_unique<Level3FileImpl>()) {}
Level3File::~Level3File() = default;

Level3File::Level3File(Level3File&&) noexcept = default;
Level3File& Level3File::operator=(Level3File&&) noexcept = default;

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

   bool dataValid = p->wmoHeader_.Parse(is);

   if (dataValid)
   {
      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << "Data Type: " << p->wmoHeader_.data_type();
      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << "ICAO:      " << p->wmoHeader_.icao();
      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << "Date/Time: " << p->wmoHeader_.date_time();
      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << "Category:  " << p->wmoHeader_.product_category();

      dataValid = p->messageHeader_.Parse(is);
   }

   if (dataValid)
   {
      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << "Code:      " << p->messageHeader_.message_code();

      dataValid = p->description_.Parse(is);
   }

   if (dataValid)
   {
      if (p->description_.IsCompressionEnabled())
      {
         size_t messageLength = p->messageHeader_.length_of_message();
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

            p->LoadBlocks(ss);
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
         p->LoadBlocks(is);
      }
   }

   return dataValid;
}

void Level3FileImpl::LoadBlocks(std::istream& is)
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Loading Blocks";

   // TODO
}

} // namespace wsr88d
} // namespace scwx
