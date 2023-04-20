#include <scwx/wsr88d/nexrad_file_factory.hpp>
#include <scwx/wsr88d/ar2v_file.hpp>
#include <scwx/wsr88d/level3_file.hpp>
#include <scwx/util/logger.hpp>

#include <fstream>
#include <sstream>

#if defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable : 4706)
#endif

#if defined(__GNUC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wdeprecated-copy"
#endif

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#if defined(__GNUC__)
#   pragma GCC diagnostic pop
#endif

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

namespace scwx
{
namespace wsr88d
{

static const std::string logPrefix_ = "scwx::wsr88d::nexrad_file_factory";
static const auto        logger_    = util::Logger::Create(logPrefix_);

std::shared_ptr<NexradFile>
NexradFileFactory::Create(const std::string& filename)
{
   logger_->debug("Create: {}", filename);

   std::shared_ptr<NexradFile> nexradFile = nullptr;
   bool                        fileValid  = true;

   std::ifstream f(filename, std::ios_base::in | std::ios_base::binary);
   if (!f.good())
   {
      logger_->warn("Could not open file for reading: {}", filename);
      fileValid = false;
   }

   if (fileValid)
   {
      nexradFile = Create(f);
   }

   return nexradFile;
}

std::shared_ptr<NexradFile> NexradFileFactory::Create(std::istream& is)
{
   std::shared_ptr<NexradFile> message = nullptr;

   std::istream*     pis      = &is;
   std::streampos    pisBegin = is.tellg();
   std::stringstream ss;
   std::string       buffer;
   bool              dataValid;

   buffer.resize(4);

   is.read(buffer.data(), 4);
   dataValid = is.good();
   is.seekg(pisBegin, std::ios_base::beg);

   if (dataValid && buffer.starts_with("\x1f\x8b"))
   {
      boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
      in.push(boost::iostreams::gzip_decompressor());
      in.push(is);

      try
      {
         std::streamsize bytesCopied = boost::iostreams::copy(in, ss);

         pis      = &ss;
         pisBegin = ss.tellg();

         ss.read(buffer.data(), 4);
         dataValid = ss.good();
         ss.seekg(pisBegin, std::ios_base::beg);

         logger_->trace("Decompressed file = {} bytes", bytesCopied);

         if (!dataValid)
         {
            logger_->warn("Error reading decompressed stream");
         }
      }
      catch (const boost::iostreams::gzip_error& ex)
      {
         logger_->warn("Error decompressing file: {}", ex.what());

         dataValid = false;
      }
   }
   else if (!dataValid)
   {
      logger_->warn("Error reading file");
   }

   if (dataValid)
   {
      if (buffer.starts_with("AR2V"))
      {
         message = std::make_shared<Ar2vFile>();
      }
      else
      {
         message = std::make_shared<Level3File>();
      }
   }

   if (message != nullptr)
   {
      dataValid = message->LoadData(*pis);

      if (!dataValid)
      {
         message = nullptr;
      }
   }

   return message;
}

} // namespace wsr88d
} // namespace scwx
