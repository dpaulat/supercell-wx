#include <scwx/util/grib_idx.hpp>

#include <gtest/gtest.h>

namespace scwx
{
namespace util
{
namespace grib_idx
{

// Real text captured live from
// noaa-gfs-bdp-pds/gfs.20260925/00/atmos/gfs.t00z.pgrb2.0p25.f000.idx
// (2026-09-26) -- not synthesized, matching this project's own
// discipline of verifying against real downloaded data.
static const std::string kGfsIdxSample =
   "1:0:d=2026092500:PRMSL:mean sea level:anl:\n"
   "2:991171:d=2026092500:CLMR:1 hybrid level:anl:\n"
   "3:1076176:d=2026092500:ICMR:1 hybrid level:anl:\n"
   "4:1312828:d=2026092500:RWMR:1 hybrid level:anl:\n";

// Real text captured live from
// noaa-nbm-grib2-pds/blend.20260925/00/core/blend.t00z.core.f001.co.grib2.idx
// (2026-09-26) -- includes the real ambiguous-shortName case flagged for
// NBM: two CAPE:surface:1 hour fcst records, one with an "ens std dev"
// qualifier and one without.
static const std::string kNbmIdxSample =
   "12:11449623:d=2026092500:TCDC:high cloud layer:1 hour fcst:\n"
   "13:12084465:d=2026092500:CAPE:surface:1 hour fcst:\n"
   "14:14154571:d=2026092500:CAPE:surface:1 hour fcst:ens std dev\n"
   "15:18527237:d=2026092500:CEIL:cloud ceiling:1 hour fcst:\n";

TEST(GribIdx, ParseIdxGfsSample)
{
   auto records = ParseIdx(kGfsIdxSample);

   ASSERT_EQ(records.size(), 4u);

   EXPECT_EQ(records[0].messageNumber, 1);
   EXPECT_EQ(records[0].byteOffset, 0);
   EXPECT_EQ(records[0].referenceTime, "d=2026092500");
   EXPECT_EQ(records[0].parameter, "PRMSL");
   EXPECT_EQ(records[0].level, "mean sea level");
   EXPECT_EQ(records[0].step, "anl");
   EXPECT_EQ(records[0].qualifier, "");

   EXPECT_EQ(records[3].messageNumber, 4);
   EXPECT_EQ(records[3].byteOffset, 1312828);
   EXPECT_EQ(records[3].parameter, "RWMR");
}

TEST(GribIdx, ParseIdxNbmQualifier)
{
   auto records = ParseIdx(kNbmIdxSample);

   ASSERT_EQ(records.size(), 4u);

   // The plain CAPE record has no qualifier.
   EXPECT_EQ(records[1].parameter, "CAPE");
   EXPECT_EQ(records[1].qualifier, "");

   // The ensemble-std-dev CAPE record shares parameter+level with the
   // plain one, distinguished only by its trailing qualifier text.
   EXPECT_EQ(records[2].parameter, "CAPE");
   EXPECT_EQ(records[2].level, "surface");
   EXPECT_EQ(records[2].qualifier, "ens std dev");
}

TEST(GribIdx, ParseIdxSkipsMalformedLines)
{
   // A blank line and a truncated line (too few fields) should be
   // skipped, not throw or abort the rest of the parse.
   const std::string idxText = "1:0:d=2026092500:PRMSL:mean sea level:anl:\n"
                                "\n"
                                "not:enough:fields\n"
                                "2:991171:d=2026092500:CLMR:1 hybrid level:anl:\n";

   auto records = ParseIdx(idxText);

   ASSERT_EQ(records.size(), 2u);
   EXPECT_EQ(records[0].parameter, "PRMSL");
   EXPECT_EQ(records[1].parameter, "CLMR");
}

TEST(GribIdx, RangeForRecordMiddleAndLast)
{
   auto records = ParseIdx(kGfsIdxSample);

   auto middle = RangeForRecord(records, 1);
   EXPECT_EQ(middle.start, 991171);
   ASSERT_TRUE(middle.end.has_value());
   EXPECT_EQ(*middle.end, 1076175);

   // The last record's range is unbounded (extends to EOF).
   auto last = RangeForRecord(records, 3);
   EXPECT_EQ(last.start, 1312828);
   EXPECT_FALSE(last.end.has_value());
}

TEST(GribIdx, FindRecordDistinguishesQualifier)
{
   auto records = ParseIdx(kNbmIdxSample);

   auto plain = FindRecord(records, "CAPE", "surface");
   ASSERT_TRUE(plain.has_value());
   EXPECT_EQ(*plain, 1u);

   auto stdDev = FindRecord(records, "CAPE", "surface", "ens std dev");
   ASSERT_TRUE(stdDev.has_value());
   EXPECT_EQ(*stdDev, 2u);

   auto missing = FindRecord(records, "CAPE", "no such level");
   EXPECT_FALSE(missing.has_value());
}

TEST(GribIdx, ToRangeHeaderBoundedAndUnbounded)
{
   ByteRange bounded {991171, 1076175};
   EXPECT_EQ(ToRangeHeader(bounded), "bytes=991171-1076175");

   ByteRange unbounded {1312828, std::nullopt};
   EXPECT_EQ(ToRangeHeader(unbounded), "bytes=1312828-");
}

} // namespace grib_idx
} // namespace util
} // namespace scwx
