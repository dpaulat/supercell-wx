#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace scwx::util::grib_idx
{

// One record from a NOMADS/wgrib2-style GRIB2 ".idx" sidecar file --
// plain text, one line per message:
// "N:byteOffset:d=YYYYMMDDHH:PARAM:LEVEL:STEP[:QUALIFIER]:". PARAM/LEVEL
// are wgrib2's own abbreviations (e.g. "TMP", "HGT"), *not* eccodes
// shortNames ("t", "gh") -- a real source of confusion this project has
// hit twice already for shortNames alone, so don't assume the two
// vocabularies line up; match against the literal idx text a real
// download showed.
struct IdxRecord
{
   int          messageNumber {};
   std::int64_t byteOffset {};
   std::string  referenceTime;
   std::string  parameter;
   std::string  level;
   std::string  step;

   // Trailing text past `step`, e.g. NBM's "ens std dev" on a handful of
   // fields that otherwise share parameter+level with a plain record.
   // Empty for the common (unqualified) case.
   std::string qualifier;
};

// Byte range a record occupies in its source .grib2 file: [start, end]
// inclusive, matching the HTTP/S3 Range header's own convention.
// `end` is unset for the last record in a file (range extends to EOF).
struct ByteRange
{
   std::int64_t                start {};
   std::optional<std::int64_t> end;
};

// Parses .idx text into records, in file order (== byteOffset order,
// which RangeForRecord() below relies on). Lines that don't match the
// expected shape are skipped rather than throwing -- this is plain text
// off S3, not a format this project controls.
std::vector<IdxRecord> ParseIdx(const std::string& idxText);

// The byte range `records[index]` occupies in the original file: up to
// (but not including) the next record's own offset, or unbounded (to
// EOF) for the last record. `records` must be in byteOffset order
// (ParseIdx()'s own output order).
ByteRange RangeForRecord(const std::vector<IdxRecord>& records,
                         std::size_t                   index);

// First record matching `parameter` and `level` exactly, and `qualifier`
// exactly (default "" matches only records with no qualifier -- the
// common case; pass the exact trailing text, e.g. "ens std dev", to pick
// a specific variant sharing the same parameter+level).
std::optional<std::size_t>
FindRecord(const std::vector<IdxRecord>& records,
          const std::string&            parameter,
          const std::string&            level,
          const std::string&            qualifier = "");

// Renders `range` as an HTTP/S3 Range header value: "bytes=start-end",
// or "bytes=start-" when `end` is unset.
std::string ToRangeHeader(const ByteRange& range);

} // namespace scwx::util::grib_idx
