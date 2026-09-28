#include <scwx/provider/mrms_data_provider.hpp>
#include <scwx/util/logger.hpp>

#include <fstream>
#include <regex>

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#if defined(__GNUC__)
// Same known issue nexrad_file_factory.cpp already works around: boost::
// iostreams::copy's internal copy_operation has an implicit copy ctor this
// GCC/Boost combination flags as deprecated (-Werror promotes it).
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wdeprecated-copy"
#endif

#include <aws/core/http/HttpRequest.h>
#include <aws/core/http/HttpResponse.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <fmt/chrono.h>

#if defined(__GNUC__)
#   pragma GCC diagnostic pop
#endif

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

namespace scwx::provider
{

static const std::string logPrefix_ = "scwx::provider::mrms_data_provider";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class MrmsDataProvider::Impl
{
public:
   explicit Impl(std::string bucketName) : bucketName_ {std::move(bucketName)}
   {
   }
   ~Impl() = default;

   // AwsNexradDataProvider stores its own copy privately with no accessor,
   // so DownloadAndDecompress() (added by this subclass, not part of the
   // base interface) needs its own.
   std::string bucketName_;
};

MrmsDataProvider::MrmsDataProvider(const std::string& product) :
    MrmsDataProvider(product, "noaa-mrms-pds", "us-east-1")
{
}

MrmsDataProvider::MrmsDataProvider(const std::string& product,
                                   const std::string& bucketName,
                                   const std::string& region) :
    AwsNexradDataProvider(product, bucketName, region),
    p(std::make_unique<Impl>(bucketName))
{
}

MrmsDataProvider::~MrmsDataProvider() = default;

std::string
MrmsDataProvider::GetPrefix(std::chrono::system_clock::time_point date)
{
   if (date < std::chrono::system_clock::time_point {})
   {
      date = std::chrono::system_clock::time_point {};
   }

   // MRMS's S3 layout: CONUS/<product>/<YYYYMMDD>/<file>.grib2.gz -- no
   // per-site subfolder, unlike NEXRAD's <site>/<product>/... layout.
   return fmt::format("CONUS/{0}/{1:%Y%m%d}/", radar_site(), fmt::gmtime(date));
}

std::chrono::system_clock::time_point
MrmsDataProvider::GetTimePointByKey(const std::string& key) const
{
   std::chrono::system_clock::time_point time {};

   // Filename format: MRMS_<Product>_<Level>_<YYYYMMDD>-<HHMMSS>.grib2.gz
   static const std::regex kTimeRegex {R"((\d{8})-(\d{6})\.grib2)"};
   std::smatch             match;

   if (std::regex_search(key, match, kTimeRegex))
   {
      const std::string dateStr = match[1].str() + " " + match[2].str();

      using namespace std::chrono;
#if (__cpp_lib_chrono < 201907L)
      using namespace date;
#endif

      std::istringstream in {dateStr};
      in >> parse("%Y%m%d %H%M%S", time);

      if (in.fail())
      {
         logger_->warn("Invalid time: \"{}\"", dateStr);
      }
   }
   else
   {
      logger_->warn("Time not parsable from key: \"{}\"", key);
   }

   return time;
}

std::shared_ptr<wsr88d::NexradFile>
MrmsDataProvider::LoadObjectByKey(const std::string& /* key */)
{
   // Not applicable: MRMS files are GRIB2, not a WSR-88D binary format.
   // Use DownloadAndDecompress() instead.
   logger_->warn(
      "LoadObjectByKey() is not applicable to MRMS data, use "
      "DownloadAndDecompress() instead");
   return nullptr;
}

std::shared_ptr<wsr88d::NexradFile> MrmsDataProvider::LoadObjectByTime(
   std::chrono::system_clock::time_point /* time */)
{
   logger_->warn(
      "LoadObjectByTime() is not applicable to MRMS data, use "
      "DownloadAndDecompress() instead");
   return nullptr;
}

std::optional<std::string> MrmsDataProvider::DownloadAndDecompress(
   const std::string&              key,
   const std::string&              outputPath,
   const DownloadProgressCallback& progressCallback)
{
   Aws::S3::Model::GetObjectRequest request;
   request.SetBucket(p->bucketName_);
   request.SetKey(key);

   // Same progress-reporting idiom as AwsNexradDataProvider::
   // DownloadObject() (which this doesn't call directly -- that helper
   // writes the raw response straight to a file, but this method needs
   // the response body itself, to decompress before ever touching disk).
   if (progressCallback)
   {
      auto bytesReceived = std::make_shared<std::int64_t>(0);

      request.SetDataReceivedEventHandler(
         [bytesReceived, progressCallback](const Aws::Http::HttpRequest*,
                                           Aws::Http::HttpResponse* response,
                                           long long                chunkSize)
         {
            *bytesReceived += chunkSize;

            std::int64_t totalBytes = -1;
            if (response != nullptr &&
                response->HasHeader(Aws::Http::CONTENT_LENGTH_HEADER))
            {
               try
               {
                  totalBytes = std::stoll(
                     response->GetHeader(Aws::Http::CONTENT_LENGTH_HEADER));
               }
               catch (const std::exception&)
               {
                  // Malformed/unparseable header -- report unknown
                  // rather than a wrong total.
               }
            }

            progressCallback(*bytesReceived, totalBytes);
         });
   }

   auto outcome = client()->GetObject(request);

   if (!outcome.IsSuccess())
   {
      logger_->warn(
         "Failed to download {}: {}", key, outcome.GetError().GetMessage());
      return std::nullopt;
   }

   auto& body = outcome.GetResultWithOwnership().GetBody();

   // Decompress to an in-memory stringstream first, then write that to
   // outputPath -- matches wsr88d::NexradFileFactory's exact pattern
   // (nexrad_file_factory.cpp) for compressed archives. boost::iostreams::
   // copy() with an std::ofstream sink directly hits a deprecated-copy
   // warning (treated as error by this project's build) in this
   // Boost/GCC combination; stringstream does not.
   std::stringstream decompressed;

   try
   {
      // MRMS objects are always gzip-compressed (.grib2.gz keys), so
      // decompress unconditionally.
      boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
      in.push(boost::iostreams::gzip_decompressor());
      in.push(body);

      std::streamsize bytesCopied = boost::iostreams::copy(in, decompressed);
      logger_->debug("Decompressed {} ({} bytes)", key, bytesCopied);
   }
   catch (const std::exception& e)
   {
      logger_->warn("Failed to decompress {}: {}", key, e.what());
      return std::nullopt;
   }

   std::ofstream out(outputPath, std::ios::binary);
   if (!out)
   {
      logger_->warn("Could not open {} for writing", outputPath);
      return std::nullopt;
   }
   out << decompressed.rdbuf();

   return outputPath;
}

} // namespace scwx::provider
