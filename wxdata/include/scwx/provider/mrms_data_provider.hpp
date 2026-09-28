#pragma once

#include <scwx/provider/aws_nexrad_data_provider.hpp>

#include <optional>
#include <string>

namespace scwx::provider
{

// Reuses AwsNexradDataProvider's S3 polling/caching/refresh machinery
// (ListObjects, Refresh, adaptive update_period()) for MRMS's public S3
// bucket -- only the prefix layout and key timestamp format actually
// differ from NEXRAD.
//
// Note: LoadObjectByKey()/LoadObjectByTime() return
// std::shared_ptr<wsr88d::NexradFile>, a WSR-88D-specific binary parse
// result -- fundamentally the wrong type for a GRIB2 file, so this class
// stubs them (returns nullptr) rather than pretending to implement them.
// Real MRMS download+decompress happens via DownloadAndDecompress()
// instead, added specifically for this class.
class MrmsDataProvider : public AwsNexradDataProvider
{
public:
   // `product` doubles as AwsNexradDataProvider's "radarSite" identifier
   // (there is no radar site for a national composite product) -- reused
   // as-is via radar_site() rather than storing a separate member.
   explicit MrmsDataProvider(const std::string& product);
   explicit MrmsDataProvider(const std::string& product,
                             const std::string& bucketName,
                             const std::string& region);
   ~MrmsDataProvider() override;

   MrmsDataProvider(const MrmsDataProvider&)            = delete;
   MrmsDataProvider& operator=(const MrmsDataProvider&) = delete;
   MrmsDataProvider(MrmsDataProvider&&)                 = delete;
   MrmsDataProvider& operator=(MrmsDataProvider&&)      = delete;

   [[nodiscard]] std::chrono::system_clock::time_point
   GetTimePointByKey(const std::string& key) const override;

   std::shared_ptr<wsr88d::NexradFile>
   LoadObjectByKey(const std::string& key) override;
   std::shared_ptr<wsr88d::NexradFile>
   LoadObjectByTime(std::chrono::system_clock::time_point time) override;

   // Downloads the S3 object for `key`, transparently gzip-decompressing
   // it (same boost::iostreams::gzip_decompressor pattern already used by
   // wsr88d::NexradFileFactory for compressed archive files -- see
   // nexrad_file_factory.cpp), and writes the result to `outputPath`,
   // optionally reporting incremental *download* progress (pre-
   // decompression byte counts -- decompression itself is fast enough
   // relative to the network transfer that it isn't worth its own
   // progress signal) via `progressCallback` (see
   // AwsNexradDataProvider::DownloadProgressCallback's own doc). Returns
   // outputPath on success, std::nullopt on failure.
   std::optional<std::string> DownloadAndDecompress(
      const std::string&              key,
      const std::string&              outputPath,
      const DownloadProgressCallback& progressCallback = nullptr);

protected:
   std::string GetPrefix(std::chrono::system_clock::time_point date) override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::provider
