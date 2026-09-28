#pragma once

#include <scwx/provider/aws_nexrad_data_provider.hpp>

#include <optional>
#include <string>

namespace scwx::provider
{

// Second concrete GRIB2 source (see MrmsDataProvider). RTMA
// (noaa-rtma-pds) bundles every field (temperature, dewpoint, wind,
// etc.) into a single hourly file rather than one S3 object per product
// like MRMS -- so unlike MrmsDataProvider, there is no per-product prefix
// here; field selection happens downstream, by GRIB shortName, once the
// file is decoded (see decode_grib's shortName argument).
//
// Also unlike MRMS, RTMA's S3 objects are plain (uncompressed) GRIB2 --
// confirmed by fetching one directly and reading it with grib_ls without
// gunzipping first -- so this class downloads as-is via DownloadRaw()
// rather than reusing MrmsDataProvider's gzip-decompressing path.
class RtmaDataProvider : public AwsNexradDataProvider
{
public:
   explicit RtmaDataProvider();
   explicit RtmaDataProvider(const std::string& bucketName,
                             const std::string& region);
   ~RtmaDataProvider() override;

   RtmaDataProvider(const RtmaDataProvider&)            = delete;
   RtmaDataProvider& operator=(const RtmaDataProvider&) = delete;
   RtmaDataProvider(RtmaDataProvider&&)                 = delete;
   RtmaDataProvider& operator=(RtmaDataProvider&&)      = delete;

   [[nodiscard]] std::chrono::system_clock::time_point
   GetTimePointByKey(const std::string& key) const override;

   std::shared_ptr<wsr88d::NexradFile>
   LoadObjectByKey(const std::string& key) override;
   std::shared_ptr<wsr88d::NexradFile>
   LoadObjectByTime(std::chrono::system_clock::time_point time) override;

   // Downloads the S3 object for `key` as-is (no decompression -- see
   // class comment) and writes it to `outputPath`, optionally reporting
   // incremental progress via `progressCallback` (see
   // AwsNexradDataProvider::DownloadProgressCallback's own doc). Returns
   // outputPath on success, std::nullopt on failure.
   std::optional<std::string>
   DownloadRaw(const std::string&              key,
               const std::string&              outputPath,
               const DownloadProgressCallback& progressCallback = nullptr);

protected:
   std::string GetPrefix(std::chrono::system_clock::time_point date) override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::provider
