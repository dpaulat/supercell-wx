#pragma once

#include <scwx/provider/nexrad_data_provider.hpp>
#include <scwx/util/grib_idx.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace Aws::S3
{
class S3Client;
} // namespace Aws::S3

namespace scwx::provider
{

/**
 * @brief AWS NEXRAD Data Provider
 */
class AwsNexradDataProvider : public NexradDataProvider
{
public:
   explicit AwsNexradDataProvider(const std::string& radarSite,
                                  const std::string& bucketName,
                                  const std::string& region);
   virtual ~AwsNexradDataProvider();

   AwsNexradDataProvider(const AwsNexradDataProvider&)            = delete;
   AwsNexradDataProvider& operator=(const AwsNexradDataProvider&) = delete;

   AwsNexradDataProvider(AwsNexradDataProvider&&)            = delete;
   AwsNexradDataProvider& operator=(AwsNexradDataProvider&&) = delete;

   // Reports one download's incremental progress: bytesReceived
   // accumulates across every call for that download; totalBytes is the
   // response's own Content-Length header, read fresh each call (cheap,
   // and robust to it not being available on the very first chunk) --
   // -1 if not present/parseable, meaning "unknown total," not "zero
   // bytes total."
   using DownloadProgressCallback =
      std::function<void(std::int64_t bytesReceived, std::int64_t totalBytes)>;

   [[nodiscard]] std::size_t cache_size() const override;

   [[nodiscard]] std::chrono::system_clock::time_point
                                      last_modified() const override;
   [[nodiscard]] std::chrono::seconds update_period() const override;

   std::string FindKey(std::chrono::system_clock::time_point time) override;
   std::string FindLatestKey() override;
   std::chrono::system_clock::time_point FindLatestTime() override;
   std::vector<std::chrono::system_clock::time_point>
   GetTimePointsByDate(std::chrono::system_clock::time_point date,
                       bool                                  update) override;
   [[nodiscard]] bool IsDateArchiveAvailable() const override;
   bool IsDateCached(std::chrono::system_clock::time_point date) override;
   std::tuple<bool, size_t, size_t>
   ListObjects(std::chrono::system_clock::time_point date) override;
   std::shared_ptr<wsr88d::NexradFile>
   LoadObjectByKey(const std::string& key) override;
   std::shared_ptr<wsr88d::NexradFile>
   LoadObjectByTime(std::chrono::system_clock::time_point time) override;
   std::pair<size_t, size_t> Refresh() override;

   /**
    * @brief Shuts down the provider and stops any in-progress network requests.
    */
   void Shutdown() noexcept override;

protected:
   std::shared_ptr<Aws::S3::S3Client> client();

   // Downloads `key` from `bucketName` to `outputPath` as-is (no
   // decompression -- callers that need that, like MrmsDataProvider,
   // decompress the result afterward), honoring Shutdown()'s own
   // cancellation (the same ContinueRequestHandler idiom LoadObjectByKey()
   // already uses) and optionally reporting incremental progress via
   // `progressCallback`. Shared by every subclass's own
   // DownloadRaw()/DownloadAndDecompress() -- centralizes what used to be
   // near-identical GetObjectRequest/S3Client boilerplate triplicated
   // across Mrms/Rtma/Rrfs, and the one place progress tracking needs to
   // be wired in at all.
   std::optional<std::string>
   DownloadObject(const std::string&              bucketName,
                  const std::string&              key,
                  const std::string&              outputPath,
                  const DownloadProgressCallback& progressCallback = nullptr);

   // Downloads only `range` of `key` to `outputPath`. Meant for GRIB2's
   // own message framing: a range computed from a NOMADS-style ".idx"
   // sidecar (see scwx::util::grib_idx) names one complete, self-
   // delimited message ("GRIB"..."7777"), so the output is a normal,
   // valid single-message .grib2 file -- decodable exactly like any
   // whole-file download, just far smaller than downloading every field
   // in the source file to get one. This is what makes activating
   // whole-globe/whole-model sources (GFS, NBM, ...) practical at all
   // within the existing per-frame cache budget -- those files run
   // hundreds of MB for every field at once, unlike RRFS/RTMA/MRMS's
   // already-small, single-purpose files.
   std::optional<std::string>
   DownloadObjectRange(const std::string&               bucketName,
                       const std::string&               key,
                       const util::grib_idx::ByteRange& range,
                       const std::string&               outputPath,
                       const DownloadProgressCallback& progressCallback = nullptr);

   // Downloads `key` fully into memory rather than to disk -- for small
   // sidecar files (namely ".idx" text) where a throwaway temp file
   // would be pure overhead.
   std::optional<std::string> DownloadObjectString(const std::string& bucketName,
                                                    const std::string& key);

   // Downloads exactly one GRIB2 message from `key` (a full-file S3
   // object with a NOMADS-style `key + ".idx"` sidecar) selected by
   // parameter/level/qualifier -- see scwx::util::grib_idx::FindRecord()
   // for how those are matched. Fetches the idx, finds the matching
   // record, and range-downloads just that message; returns std::nullopt
   // if the idx can't be fetched or no record matches. On success,
   // `outputPath` holds a normal, valid single-message .grib2 file (see
   // DownloadObjectRange()'s own comment on why that's true).
   std::optional<std::string>
   DownloadGribMessageByIndex(const std::string&              bucketName,
                              const std::string&               key,
                              const std::string&               parameter,
                              const std::string&               level,
                              const std::string&               qualifier,
                              const std::string&               outputPath,
                              const DownloadProgressCallback& progressCallback = nullptr);

   virtual std::string
   GetPrefix(std::chrono::system_clock::time_point date) = 0;

private:
   class Impl;
   std::unique_ptr<Impl> p;

   std::optional<std::string>
   DownloadObjectImpl(const std::string&                              bucketName,
                      const std::string&                              key,
                      const std::optional<util::grib_idx::ByteRange>& range,
                      const std::string&                              outputPath,
                      const DownloadProgressCallback&                 progressCallback);
};

} // namespace scwx::provider
