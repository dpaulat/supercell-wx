#include <scwx/qt/manager/grib_manager.hpp>
#include <scwx/qt/manager/status_manager.hpp>
#include <scwx/qt/manager/timeline_manager.hpp>
#include <scwx/qt/map/grib_frame_info.hpp>
#include <scwx/qt/settings/unit_settings.hpp>
#include <scwx/qt/types/unit_types.hpp>
#include <scwx/provider/mrms_data_provider.hpp>
#include <scwx/provider/rrfs_data_provider.hpp>
#include <scwx/provider/rtma_data_provider.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <algorithm>
#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <utility>

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <fmt/format.h>

#include <QCoreApplication>
#include <QProcess>
#include <QTimer>

namespace scwx::qt::manager
{

static const std::string logPrefix_ = "scwx::qt::manager::grib_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

namespace
{

// Path to the out-of-process eccodes decode helper (see
// grib-helper/README.md for why it isn't linked directly into wxdata).
// Built alongside supercell-wx and installed next to it when eccodes was
// found at configure time (see grib-helper/CMakeLists.txt) -- resolved
// lazily, not at static-init time, since QCoreApplication::
// applicationDirPath() needs the QApplication instance main() constructs
// before this is ever actually called.
const std::string& DecodeGribPath()
{
   static const std::string path = (QCoreApplication::applicationDirPath() +
#if defined(_WIN32)
                                    "/decode_grib.exe"
#else
                                    "/decode_grib"
#endif
                                    )
                                      .toStdString();
   return path;
}

// See map::GetGribDataDirectory() -- same lazy-static reasoning applies
// here (ApplicationPaths::Initialize() must have already run).
const std::string& DownloadDir()
{
   static const std::string dir = map::GetGribDataDirectory().string();
   return dir;
}

const std::string& CacheDir()
{
   static const std::string dir = DownloadDir() + "/cache";
   return dir;
}

} // namespace

// Unlike MRMS -- one S3 object per product -- RTMA and RRFS both bundle
// many fields into one file, so there's no per-field S3 path to select;
// every product in a given category's table resolves to the *same* S3
// key for a given time, and field selection happens downstream, by GRIB
// shortName, once decode_grib has the file (see decode_grib's shortName
// CLI argument). Unlike RTMA though, RRFS bundles fields into *two* files
// (2dfld/prslev, see RrfsDataProvider's class comment) -- kRrfsProducts_
// entries are 2dfld-only for now (RrfsDataProvider only ever resolves
// the 2dfld file); prslev support is a real follow-up, not built yet.
// (A `Source` enum used to
// live here to pick download mechanics per product; removed once
// map::GribCategory itself became one-per-family -- category alone
// determines it now, see MakeProvider.)

// What physical quantity a product's raw value represents, so the
// Shift-hover data tooltip can convert it to whatever the user has
// configured in Settings > Units, the same way RadarProductLayer's own
// tooltip already does for distance/height. None means there's no
// user-configurable alternative (dBZ is dBZ; the same is true of MRMS's
// rotation track, which isn't even in a clean physical unit to begin
// with) -- shown as ProductConfig::units natively, unconverted.
enum class PhysicalQuantity
{
   None,
   TemperatureKelvin,
   SpeedMetersPerSecond,
   AccumulationMillimeters,
   PressurePascals,
};

// Curated set of MRMS/RTMA/RRFS products (confirmed against the real
// noaa-mrms-pds/noaa-rtma-pds/noaa-rrfs-ops-pds S3 buckets), not any
// source's full catalog -- MRMS entries picked to include products with
// no per-site-radar equivalent (rotation track, hail, precip), so this
// doesn't just read as redundant with radar. displayName is what
// GribDockWidget's dropdown shows; s3Product is the literal
// CONUS/<s3Product>/ folder name
// (Mrms only); shortName is the GRIB shortName decode_grib should select
// out of the bundled file (Rtma/Rrfs only). Which table a product lives
// in (kMrmsProducts_/kRtmaProducts_/kRrfsProducts_) is itself what used
// to be the `source` field -- see Products(GribCategory).
struct ProductConfig
{
   std::string      displayName;
   std::string      s3Product;
   std::string      shortName;
   float            colorOffset;
   float            colorScale;
   float            noDataThreshold;
   PhysicalQuantity quantity;
   std::string      units; // native GRIB physical unit -- the tooltip
                           // shows this as-is only when quantity is None;
                           // otherwise GribManager::FormatValue() converts
                           // and picks the unit's own abbreviation instead
                           // (see quantity's own comment).

   // 0 (default) means the normal palette-fill rendering; a nonzero value
   // tells decode_grib (see its own contourInterval CLI arg) to bake an
   // isoline-mode header into the frame instead, in this product's own
   // physical unit (e.g. 400 for MSLP's 400 Pa / 4 hPa synoptic
   // convention). Only meaningful alongside a non-empty shortName --
   // decode_grib's CLI requires shortName before contourInterval, which
   // every current/likely contour candidate (RTMA/RRFS bundled fields)
   // already passes anyway.
   float contourInterval = 0.0f;

   // Empty (default): the normal single-message decode, selecting
   // `shortName` out of the bundled file. Non-empty (e.g. "stp"): this
   // product is computed from several messages in the same file via
   // decode_grib's `--derived <name>` mode instead (see decode_grib.cpp's
   // own RunDerived/ComputeStp) -- `shortName` is unused in that case,
   // decode_grib's derived path has its own fixed, internally-hardcoded
   // field list per index name.
   std::string derivedIndex;

   // Empty typeOfLevel (default) / -1 topLevel/bottomLevel (default)
   // means "shortName alone is unambiguous," matching every product
   // that predates these fields. Several real RRFS fields (cape/cin/
   // hlcy/vucsh/vvcsh/tcc/rare) share a shortName with another level/
   // layer in the same 2dfld file and need these set -- see decode_grib's
   // own FindMessage for the exact mechanism, confirmed against a real
   // downloaded file for every product that sets them, not guessed.
   std::string typeOfLevel;
   long        topLevel    = -1;
   long        bottomLevel = -1;

   // -1 (default, "don't care") for every product above. RRFS's two
   // "tp" (Total Precipitation) messages share identical typeOfLevel/
   // topLevel/bottomLevel, so those three can't disambiguate them --
   // startStep/lengthOfTimeRange are the actual discriminators (see
   // decode_grib's FindMessage for the exact mechanism and "1-Hour
   // Precipitation"/"Total Precipitation" below for the only two
   // products that set them).
   long startStep         = -1;
   long lengthOfTimeRange = -1;

   // Rrfs-only (meaningless for Mrms/Rtma, left at the default) --
   // TwoDField (default) reads the 2dfld file every existing RRFS product
   // came from; PressureLevel reads the *other* per-cycle file RRFS
   // publishes (isobaric-level fields -- 500mb/700mb/etc. HGT/TMP/wind),
   // a genuinely different file with its own, different hourly-
   // availability rule (see provider::RrfsFileFamily's own doc). Read by
   // MakeProvider() once, at provider construction -- unlike cycle/
   // forecast-hour, which the user can change live via the run/hour
   // picker, this is fixed for a product's whole lifetime.
   provider::RrfsFileFamily rrfsFileFamily =
      provider::RrfsFileFamily::TwoDField;
};

// clang-format off

// MRMS: radar mosaic products, map::GribCategory::Mrms.
static const std::vector<ProductConfig> kMrmsProducts_ {
   // dBZ, matches res/palettes/wct/DR.pal's defined range (-20 to 75).
   // Below 0 dBZ (real calm-air returns, and MRMS's -999 "no coverage"
   // sentinel) is not rendered.
   {"Composite Reflectivity",
    "MergedReflectivityQCComposite_00.50", "", -20.0f, 95.0f, 0.0f,
    PhysicalQuantity::None, "dBZ", 0.0f, "", "", -1, -1},

   // Each is a real MRMS product with no per-site-radar equivalent
   // (unlike composite reflectivity, which single-site NEXRAD already
   // shows) -- that's deliberate, see the comment above this table.
   // Unlike reflectivity, 0 is a real, meaningful data value for all
   // three (0 rotation, 0mm hail, 0mm rain isn't "no data"), so
   // noDataThreshold sits at -1.0 for all three rather than at 0 like
   // reflectivity's does -- verified against live data (2026-09-20) that
   // MESH/QPE actually use -3 as their sentinel, not -999 like
   // reflectivity; -1.0 still correctly separates it from legitimate
   // values either way, since real magnitude-type data for all three
   // can't be negative at all.

   // Rotational velocity difference. Verified against live data
   // (2026-09-20, an active severe weather day) that the file's raw
   // values are pre-scaled by ~1000 from the textbook s^-1 unit -- an
   // observed real max of 22 corresponds to an extreme ~0.022 s^-1
   // event, not 22 s^-1. Scale to 30 (raw units) so that real event
   // fills most of the ramp with a little headroom, not clipped near
   // the top.
   {"Rotation Track (30 min)",
    "RotationTrackML30min_00.50", "", 0.0f, 30.0f, -1.0f,
    PhysicalQuantity::None, "raw units (x1000 /s)", 0.0f, "", "", -1, -1},

   // Maximum Estimated Size of Hail, mm. Severe criteria (1 in) is
   // 25.4mm, significant severe (2 in) is 50.8mm; extreme record cases
   // exceed 100mm. Verified against live data: real current max 61.8mm,
   // comfortably inside this range. Quantity is Accumulation (ties to the
   // same Settings > Units > Accumulation the user already has for radar
   // precip totals) rather than a dedicated "hail size" unit -- both are
   // just "millimeters of something", and Accumulation's Inches/
   // Millimeters options are exactly the right pair either way.
   {"Max Hail Size (60 min)",
    "MESH_Max_60min_00.50", "", 0.0f, 100.0f, -1.0f,
    PhysicalQuantity::AccumulationMillimeters, "mm", 0.0f, "", "", -1, -1},

   // 1-hour radar-only precip estimate, mm. Verified against live data:
   // real current max was 71.4mm, already exceeding an initial 50mm
   // guess -- scaled to 80 for headroom above an observed real event
   // rather than clipping it.
   {"1-Hour Precip Estimate",
    "RadarOnly_QPE_01H_00.00", "", 0.0f, 80.0f, -1.0f,
    PhysicalQuantity::AccumulationMillimeters, "mm", 0.0f, "", "", -1, -1},
};

// RTMA: rolling surface analysis, map::GribCategory::Rtma. noDataThreshold
// sits far below every real value (rather than at a sentinel like MRMS's)
// because RTMA is a QC'd, full-CONUS surface analysis with no coverage
// gaps -- confirmed via grib_ls against a real file: numberOfMissing=0
// for every field -- so there's no sentinel to filter, just headroom
// under legitimately-always-positive values (Kelvin, wind speed).
static const std::vector<ProductConfig> kRtmaProducts_ {
   // 2m temperature, Kelvin. Verified against live data: real range
   // 270.15-314.64K; scaled to 260-325K for seasonal/regional headroom.
   {"2m Temperature", "", "2t", 260.0f, 65.0f, -999.0f,
    PhysicalQuantity::TemperatureKelvin, "K", 0.0f, "", "", -1, -1},

   // 2m dewpoint, Kelvin. Verified against live data: real range
   // 242.07-302.49K; scaled to 235-310K for headroom (Gulf Coast summer
   // dewpoints can approach the top of this range).
   {"2m Dewpoint", "", "2d", 235.0f, 75.0f, -999.0f,
    PhysicalQuantity::TemperatureKelvin, "K", 0.0f, "", "", -1, -1},

   // 10m wind speed, m/s. Verified against live data: real range
   // 0-26 m/s; scaled to 0-30 for headroom above an observed calm day.
   {"10m Wind Speed", "", "10si", 0.0f, 30.0f, -999.0f,
    PhysicalQuantity::SpeedMetersPerSecond, "m/s", 0.0f, "", "", -1, -1},

   // 10m wind gust, m/s (shortName i10fg). Flagged as the most
   // operationally useful unsurfaced RTMA field -- gusts are what
   // actually matter for damage/warning criteria, not sustained speed.
   // Verified against live data: real range 0-28.4 m/s; scaled to 0-40
   // for headroom toward severe/damaging gust events (that one snapshot
   // wasn't an active severe day), same reasoning as 10m Wind Speed above.
   {"10m Wind Gust", "", "i10fg", 0.0f, 40.0f, -999.0f,
    PhysicalQuantity::SpeedMetersPerSecond, "m/s", 0.0f, "", "", -1, -1},

   // Surface visibility, metres (shortName vis). Verified against live
   // data: real range 1-16000 m -- 16000 is the field's own ceiling
   // (NWP's standard "unlimited visibility" cap, not this particular
   // day's actual max), so colorScale is set to that physical cap
   // directly rather than adding headroom above an observed value like
   // every other field here does. Quantity is None (raw metres) for now,
   // not a proper distance-unit conversion -- no PhysicalQuantity::
   // Distance category exists yet (UnitSettings has no visibility/range
   // unit either), scoped as a follow-up rather than added tonight.
   {"Visibility", "", "vis", 0.0f, 16000.0f, -999.0f,
    PhysicalQuantity::None, "m", 0.0f, "", "", -1, -1},
};

// RRFS: forecast model, map::GribCategory::Rrfs. Phase 1 scope: all
// 2dfld, fixed at RrfsDataProvider's resolved synoptic-cycle F000, no
// forecast-hour selection yet. Same no-coverage-gaps convention as RTMA
// above -- RRFS's fields are full model grids too.
static const std::vector<ProductConfig> kRrfsProducts_ {
   // Mean sea level pressure (ETA reduction), Pa. Verified against live
   // data on two separate days: 100203-103017 Pa (2026-09-20) and
   // 99345.6-103127 Pa (2026-09-25), both comfortably inside a
   // 97000-107000 Pa (970-1070 hPa) headroom range toward a deep
   // continental low and a strong winter high. Rendered as isolines every
   // 400 Pa (4 hPa, the standard synoptic MSLP contour interval), not a
   // solid fill -- Phase 1 shipped as a plain fill deliberately, to prove
   // the provider/S3-key/decode slice end to end first; now that it has,
   // this is the "revisit the deferred contour work" phase. colorOffset/
   // colorScale below are vestigial in contour mode (the shader ignores
   // them once contourInterval is set) but left as-is rather than zeroed,
   // in case a fill/contour toggle is ever added.
   {"Mean Sea Level Pressure", "", "mslet", 97000.0f, 10000.0f, -999.0f,
    PhysicalQuantity::PressurePascals, "Pa", 400.0f, "", "", -1, -1},

   // Composite lightning threat, dimensionless (NCEP's own composite
   // index, not a literal flash count) -- "entire atmosphere, 0-1 hour
   // max fcst". Verified against live data: real range 0-36.46; scaled
   // to 0-50 for headroom above an observed active day. Note: RRFS also
   // carries LTNGSD (lightning strike density, two near-surface levels)
   // in the same file, but its shortName resolves to "unknown" in
   // eccodes (no definition table entry for this parameter combination
   // yet) -- decode_grib selects by shortName, so that field isn't
   // usable this way; LTNG is the one that works.
   {"Lightning Threat", "", "ltng", 0.0f, 50.0f, -999.0f,
    PhysicalQuantity::None, "index", 0.0f, "", "", -1, -1},

   // Significant Tornado Parameter, fixed-layer form (Thompson et al.
   // 2003) -- SPC's documented predecessor to today's effective-layer
   // default; true effective-layer STP needs a full vertical-profile
   // parcel test RRFS doesn't ship as ready fields, deliberately out of
   // scope here (see decode_grib.cpp's ComputeStp for the full formula
   // and field-selection notes). Computed, not decoded -- shortName is
   // unused (empty); decode_grib's `--derived stp` mode has its own
   // fixed, internally-hardcoded field list. Verified against live data:
   // real range -1.11 to 2.26 on a quiet (non-severe) day; scaled to
   // -2 to 6 for headroom toward a genuinely favorable severe-weather
   // setup, which this one observed day wasn't.
   {"STP (Fixed-Layer)", "", "", -2.0f, 8.0f, -9000.0f,
    PhysicalQuantity::None, "index", 0.0f, "stp", "", -1, -1},

   // Everything below verified against the same real downloaded 2dfld
   // file as MSLET/STP above (2026-09-25) -- a full field inventory,
   // 262 messages, confirmed how few of them were actually exposed
   // before this pass. Several share a shortName with another level/
   // layer in the same file (see ProductConfig::typeOfLevel's own
   // comment) -- every entry below that sets typeOfLevel/topLevel/
   // bottomLevel needed it to resolve to the intended message, confirmed
   // by decoding each one directly and checking its range matched.

   // Model-derived composite reflectivity, three fixed levels -- RRFS's
   // `rare` field, not an MRMS-style true composite (max across all
   // tilts); each level is its own product since eccodes has no single
   // "composite" message for it. Same -20/95 range as MRMS reflectivity
   // (kMrmsProducts_ above) so it reads on the same visual scale.
   {"Simulated Reflectivity (1km AGL)", "", "rare", -20.0f, 95.0f, -999.0f,
    PhysicalQuantity::None, "dBZ", 0.0f, "", "heightAboveGround", 1000, 1000},
   {"Simulated Reflectivity (4km AGL)", "", "rare", -20.0f, 95.0f, -999.0f,
    PhysicalQuantity::None, "dBZ", 0.0f, "", "heightAboveGround", 4000, 4000},
   {"Simulated Reflectivity (-10C level)", "", "rare", -20.0f, 95.0f, -999.0f,
    PhysicalQuantity::None, "dBZ", 0.0f, "", "isothermal", 263, 263},

   // Vertically Integrated Liquid, kg/m^2. Verified: real range
   // 0.001-197.3; scaled to 0-220 for headroom.
   {"VIL", "", "veril", 0.0f, 220.0f, -999.0f,
    PhysicalQuantity::None, "kg/m^2", 0.0f, "", "", -1, -1},

   // RRFS's own forecast surface visibility, metres -- distinct from
   // RTMA's analysis-only vis (kRtmaProducts_ above). Verified: real
   // range 23-86586 m, notably not capped at 16000m the way RTMA's own
   // vis is -- this field's own ceiling is higher, so scaled to that
   // observed max plus headroom rather than reusing RTMA's constant.
   {"Visibility", "", "vis", 0.0f, 90000.0f, -999.0f,
    PhysicalQuantity::None, "m", 0.0f, "", "", -1, -1},

   // RRFS's own forecast 10m wind gust, m/s -- distinct from RTMA's
   // analysis-only gust (kRtmaProducts_ above, shortName i10fg).
   // Verified: real range 0-39.8; scaled to 0-45 for headroom.
   {"Wind Gust", "", "gust", 0.0f, 45.0f, -999.0f,
    PhysicalQuantity::SpeedMetersPerSecond, "m/s", 0.0f, "", "", -1, -1},

   // Surface pressure, Pa -- station pressure, not MSLP (see MSLET
   // above for that). Verified: real range 64432-102960 Pa (terrain
   // drives this range far more than weather does, same as RTMA's own
   // unsurfaced sp field); scaled to 60000-105000 for headroom.
   {"Surface Pressure", "", "sp", 60000.0f, 45000.0f, -999.0f,
    PhysicalQuantity::PressurePascals, "Pa", 0.0f, "", "", -1, -1},

   // Instantaneous precipitation rate, kg/m^2/s (== mm/s). Verified:
   // real range 0-0.0791; scaled to 0-0.1 for headroom. Quantity is None
   // (raw rate), not AccumulationMillimeters -- that setting is for
   // totals, converting a rate through it would be wrong.
   {"Precipitation Rate", "", "prate", 0.0f, 0.1f, -999.0f,
    PhysicalQuantity::None, "kg/m^2/s", 0.0f, "", "", -1, -1},

   // 1-hour accumulated precipitation, kg/m^2 (== mm). RRFS's 2dfld file
   // carries *two* "tp" messages sharing identical surface/level-0
   // metadata -- this one and "Total Precipitation" below -- disambiguated
   // by lengthOfTimeRange (always 1 here, regardless of forecast hour;
   // see decode_grib's FindMessage for the full mechanism, confirmed live
   // via grib_ls against a real forecast-hour-5 file, not assumed).
   // Verified: real range 0-107.8 at that hour; scaled to 0-120 for
   // headroom.
   {"1-Hour Precipitation", "", "tp", 0.0f, 120.0f, -999.0f,
    PhysicalQuantity::AccumulationMillimeters, "mm", 0.0f, "", "", -1, -1,
    -1, 1},

   // Accumulated precipitation since the run's own start (F000), kg/m^2
   // (== mm) -- the *other* "tp" message, disambiguated by startStep
   // (always 0 here, the run-total's own defining trait, vs. the 1-hour
   // field's forecast-hour-dependent startStep). Verified: real range
   // 0-139.8 at forecast hour 5 -- scaled to 0-300 for headroom, but
   // real cost not silently absorbed: this keeps growing with lead time
   // (it is a running total over the whole forecast, up to 84h), so a
   // long-enough run's true max could still exceed this; only checked at
   // one (early) forecast hour, not the full range.
   {"Total Precipitation", "", "tp", 0.0f, 300.0f, -999.0f,
    PhysicalQuantity::AccumulationMillimeters, "mm", 0.0f, "", "", -1, -1,
    0, -1},

   // -- prslev-backed products below (RRFS's *other* per-cycle file,
   // isobaric-level fields -- see provider::RrfsFileFamily's own doc for
   // its real, different hourly-availability rule from every product
   // above). Every field below shares typeOfLevel "isobaricInhPa" with
   // topLevel==bottomLevel==level (confirmed live: a single isobaric
   // level, not a layer, reports identical top/bottom -- so the existing
   // typeOfLevel/topLevel/bottomLevel disambiguation already handles
   // this with no decode_grib changes). eccodes' own shortNames here
   // ("gh"/"t"/"u"/"v") differ from 2dfld's surface-field naming
   // convention ("hgt" doesn't exist at all here -- confirmed live, not
   // assumed from wxqt's own docs, which named a different field "hgt"
   // too, see [[supercell-wx-rrfs-plan]] for that same lesson learned
   // once already with STP's LCL height).

   // 500mb geopotential height, metres -- rendered as isolines every 60m
   // (the standard US synoptic 500mb contour interval, 6 decameters),
   // matching MSLET's own contour treatment above for the same
   // "troughs/ridges read as lines, not a color wash" reasoning.
   // Verified: real range 5493.75-5952.81m at one forecast hour;
   // colorOffset/colorScale below are vestigial in contour mode (see
   // MSLET's own comment), left non-zero anyway.
   {"500mb Height", "", "gh", 5000.0f, 1500.0f, -999.0f,
    PhysicalQuantity::None, "m", 60.0f, "", "isobaricInhPa", 500, 500, -1, -1,
    provider::RrfsFileFamily::PressureLevel},

   // 500mb temperature, K. Verified: real range 248.1-273.8K; scaled to
   // 240-290K for headroom.
   {"500mb Temperature", "", "t", 240.0f, 50.0f, -999.0f,
    PhysicalQuantity::TemperatureKelvin, "K", 0.0f, "", "isobaricInhPa",
    500, 500, -1, -1, provider::RrfsFileFamily::PressureLevel},

   // 700mb temperature, K -- SHIP's own T700 input (see ComputeStp's
   // class-comment-adjacent notes on SHIP being blocked on this exact
   // field); useful as its own product regardless of whether SHIP itself
   // gets built on top of it. Verified: real range 264.7-289.4K; scaled
   // to 255-300K for headroom.
   {"700mb Temperature", "", "t", 255.0f, 45.0f, -999.0f,
    PhysicalQuantity::TemperatureKelvin, "K", 0.0f, "", "isobaricInhPa",
    700, 700, -1, -1, provider::RrfsFileFamily::PressureLevel},

   // 500mb wind speed, m/s -- sqrt(u^2+v^2) via decode_grib's `--derived
   // wind500`, same shape as shear6/wind10 above (see
   // ComputeVectorMagnitude). Raw u/v components aren't exposed as their
   // own products here -- a fill of signed velocity components isn't
   // meaningfully readable the way a scalar speed is; u/v as wind-barb
   // inputs is the eventual point of having this level at all (see
   // [[supercell-wx-grib-extension]]'s "wind-barb default-pairing" idea),
   // not yet built. Verified: real range 0.004-44.5 m/s; scaled to 0-60
   // for headroom.
   {"500mb Wind Speed", "", "", 0.0f, 60.0f, -999.0f,
    PhysicalQuantity::SpeedMetersPerSecond, "m/s", 0.0f, "wind500", "", -1,
    -1, -1, -1, provider::RrfsFileFamily::PressureLevel},

   // Significant Hail Parameter (SHIP), SPC mesoanalysis form -- the one
   // product that needed prslev to exist at all (T500/T700, see
   // decode_grib.cpp's ComputeShip for the full formula/field-selection
   // notes). Computed, not decoded -- shortName is unused (empty);
   // decode_grib's `--derived ship` mode has its own fixed field list,
   // spanning *two* input files (2dfld + prslev), which
   // FetchShipSelection()/ApplyShipDownload() handle -- this product
   // never reaches RequestFrame()/QueueDownload()/ApplyCachedDownload().
   // rrfsFileFamily is left at its TwoDField default -- meaningless here,
   // since FetchShipSelection() bypasses GetPrefix()/FindKey() entirely
   // for both of its own inputs. Verified against live data: real range
   // -0.0007 to 1.24 on a quiet (non-severe) day; scaled to 0-6 for
   // headroom toward SPC's own documented "very high" (>4) ceiling,
   // which this one observed day wasn't close to, same reasoning as
   // STP's own headroom above.
   {"SHIP", "", "", 0.0f, 6.0f, -9000.0f, PhysicalQuantity::None, "index",
    0.0f, "ship", "", -1, -1},

   // Planetary boundary layer height, metres AGL. Verified: real range
   // 19-2247 m; scaled to 0-2500 for headroom.
   {"Boundary Layer Height", "", "blh", 0.0f, 2500.0f, -999.0f,
    PhysicalQuantity::None, "m", 0.0f, "", "", -1, -1},

   // Precipitable water, kg/m^2 -- numerically equals mm of liquid
   // water equivalent (1 kg/m^2 == 1 mm), so AccumulationMillimeters is
   // physically correct here, not just a unit-name coincidence.
   // Verified: real range 1.6-83.7; scaled to 0-90 for headroom.
   {"Precipitable Water", "", "pwat", 0.0f, 90.0f, -999.0f,
    PhysicalQuantity::AccumulationMillimeters, "mm", 0.0f, "", "", -1, -1},

   // Cloud ceiling, metres AGL. Verified: real range 28-15993; scaled to
   // 0-16000, matching RTMA's own ceiling convention (kRtmaProducts_
   // doesn't have one yet, but this mirrors its vis field's cap).
   {"Ceiling", "", "ceil", 0.0f, 16000.0f, -999.0f,
    PhysicalQuantity::None, "m", 0.0f, "", "", -1, -1},

   // Total cloud cover, %. Two "tcc" messages exist in the same file
   // (boundaryLayerCloudLayer and atmosphereSingleLayer) -- picked
   // atmosphereSingleLayer as the more standard "whole column" total,
   // disambiguated explicitly rather than trusting message order.
   {"Total Cloud Cover", "", "tcc", 0.0f, 100.0f, -999.0f,
    PhysicalQuantity::None, "%", 0.0f, "", "atmosphereSingleLayer", 0, 0},
   {"Low Cloud Cover", "", "lcc", 0.0f, 100.0f, -999.0f,
    PhysicalQuantity::None, "%", 0.0f, "", "", -1, -1},
   {"Mid Cloud Cover", "", "mcc", 0.0f, 100.0f, -999.0f,
    PhysicalQuantity::None, "%", 0.0f, "", "", -1, -1},
   {"High Cloud Cover", "", "hcc", 0.0f, 100.0f, -999.0f,
    PhysicalQuantity::None, "%", 0.0f, "", "", -1, -1},

   // Surface-based CAPE/CIN, J/kg -- the file also carries 3 mixed-layer
   // CAPE/CIN variants (90/180/255 mb) sharing the same "cape"/"cin"
   // shortName; "surface" disambiguates to the SB (not MU) variant.
   // Verified: CAPE 0-5006, CIN -874-0; scaled with headroom.
   {"SBCAPE", "", "cape", 0.0f, 5500.0f, -999.0f,
    PhysicalQuantity::None, "J/kg", 0.0f, "", "surface", 0, 0},
   {"SBCIN", "", "cin", -1000.0f, 1000.0f, -999.0f,
    PhysicalQuantity::None, "J/kg", 0.0f, "", "surface", 0, 0},

   // Most-unstable CAPE/CIN (180mb mixed layer -- the variant SHIP's own
   // formula uses, see decode_grib.cpp; SHIP itself stays deferred until
   // prslev support exists, but MUCAPE/MUCIN are useful fields alone).
   // Verified: CAPE 0-4224, CIN -1364-0.
   {"MUCAPE", "", "cape", 0.0f, 4600.0f, -999.0f,
    PhysicalQuantity::None, "J/kg", 0.0f, "", "pressureFromGroundLayer",
    18000, 0},
   {"MUCIN", "", "cin", -1500.0f, 1500.0f, -999.0f,
    PhysicalQuantity::None, "J/kg", 0.0f, "", "pressureFromGroundLayer",
    18000, 0},

   // Storm-relative helicity, m^2/s^2 -- "hlcy" appears at both 0-1km and
   // 0-3km in the same file, disambiguated by bottomLevel (topLevel is
   // always the surface, 0). Verified: 0-1km range -210.4-433.7, 0-3km
   // range -460-1160.
   {"0-1km SRH", "", "hlcy", -250.0f, 750.0f, -999.0f,
    PhysicalQuantity::None, "m^2/s^2", 0.0f, "", "heightAboveGroundLayer",
    1000, 0},
   {"0-3km SRH", "", "hlcy", -500.0f, 1700.0f, -999.0f,
    PhysicalQuantity::None, "m^2/s^2", 0.0f, "", "heightAboveGroundLayer",
    3000, 0},

   // 0-6km bulk shear magnitude, m/s -- computed, not decoded (see
   // decode_grib's `--derived shear6`, sqrt(vucsh^2+vvcsh^2) at the
   // 0-6000m heightAboveGroundLayer, the same input SHIP/STP's own
   // shear term uses). Verified against live data: real range 0-54.1;
   // scaled to 0-60 for headroom.
   {"0-6km Shear", "", "", 0.0f, 60.0f, -999.0f,
    PhysicalQuantity::SpeedMetersPerSecond, "m/s", 0.0f, "shear6", "", -1,
    -1},

   // RRFS's own forecast 10m wind speed, m/s -- computed from the
   // native 10u/10v components (no native speed field exists the way
   // RTMA's 10si does), distinct from RTMA's analysis-only 10m Wind
   // Speed (kRtmaProducts_ above). Verified: real range 0-32; scaled to
   // 0-35 for headroom.
   {"10m Wind Speed", "", "", 0.0f, 35.0f, -999.0f,
    PhysicalQuantity::SpeedMetersPerSecond, "m/s", 0.0f, "wind10", "", -1,
    -1},

   // Simulated GOES-16 ABI brightness temperature, one product per band
   // (7-16, all "unknown"-free unique shortNames, no disambiguation
   // needed). Verified against live data: each band's own real range
   // used directly as colorOffset/colorScale (no added headroom -- these
   // are already full observed ranges for a real day, revisit if a
   // future day is seen to exceed them).
   {"Sat Band 7 (3.9um)", "", "SBTA167", 190.0f, 120.0f, -999.0f,
    PhysicalQuantity::TemperatureKelvin, "K", 0.0f, "", "", -1, -1},
   {"Sat Band 8 (6.2um)", "", "SBTA168", 190.0f, 75.0f, -999.0f,
    PhysicalQuantity::TemperatureKelvin, "K", 0.0f, "", "", -1, -1},
   {"Sat Band 9 (6.9um)", "", "SBTA169", 190.0f, 80.0f, -999.0f,
    PhysicalQuantity::TemperatureKelvin, "K", 0.0f, "", "", -1, -1},
   {"Sat Band 10 (7.3um)", "", "SBTA1610", 190.0f, 90.0f, -999.0f,
    PhysicalQuantity::TemperatureKelvin, "K", 0.0f, "", "", -1, -1},
   {"Sat Band 11 (8.4um)", "", "SBTA1611", 190.0f, 115.0f, -999.0f,
    PhysicalQuantity::TemperatureKelvin, "K", 0.0f, "", "", -1, -1},
   {"Sat Band 12 (9.6um)", "", "SBTA1612", 205.0f, 75.0f, -999.0f,
    PhysicalQuantity::TemperatureKelvin, "K", 0.0f, "", "", -1, -1},
   {"Sat Band 13 (10.3um)", "", "SBTA1613", 190.0f, 115.0f, -999.0f,
    PhysicalQuantity::TemperatureKelvin, "K", 0.0f, "", "", -1, -1},
   {"Sat Band 14 (11.2um)", "", "SBTA1614", 190.0f, 115.0f, -999.0f,
    PhysicalQuantity::TemperatureKelvin, "K", 0.0f, "", "", -1, -1},
   {"Sat Band 15 (12.3um)", "", "SBTA1615", 190.0f, 112.0f, -999.0f,
    PhysicalQuantity::TemperatureKelvin, "K", 0.0f, "", "", -1, -1},
   {"Sat Band 16 (13.3um)", "", "SBTA1616", 190.0f, 92.0f, -999.0f,
    PhysicalQuantity::TemperatureKelvin, "K", 0.0f, "", "", -1, -1},
};
// clang-format on

const std::vector<ProductConfig>& Products(map::GribCategory category)
{
   switch (category)
   {
   case map::GribCategory::Mrms:
      return kMrmsProducts_;
   case map::GribCategory::Rtma:
      return kRtmaProducts_;
   case map::GribCategory::Rrfs:
   default:
      return kRrfsProducts_;
   }
}

// User asked for a 4 minute default -- roughly half MRMS's native ~2
// minute cadence, meant for "glance at the national picture" rather than
// chasing every update.
static constexpr int kPollIntervalMs_ = 4 * 60 * 1000;

// Size-based, not count-based: a cached download ranges from ~1.5-2MB
// (MRMS) through ~84MB (RTMA, one file for all 13 fields) up to ~320MB
// (RRFS's 2dfld file) -- a fixed file-count cap sized for the small end
// would let the large end blow past any reasonable disk budget, and a cap
// sized for the large end would barely bound the small end at all. Must
// stay comfortably above one full RRFS forecast-hour prefetch's own
// footprint (see GribManager::PrefetchRrfsForecastHourRange(), up to ~27GB
// for a 6-hourly cycle's 84 hours) -- otherwise later hours in the same
// prefetch pass would evict earlier ones before playback ever reaches
// them, defeating the whole point of prefetching. 40GB gives that pass
// headroom to complete plus room for MRMS/RTMA's own loop ranges
// alongside it, while still being a real, finite bound rather than
// unbounded growth across a long-running session. Oldest-by-mtime entries
// are evicted first once the total exceeds this.
static constexpr std::uintmax_t kMaxCacheSizeBytes_ =
   40ULL * 1024 * 1024 * 1024;

// Mrms/Rtma/RrfsDataProvider share every method GribManager's Poll()/
// FetchArchiveFrame() paths need (Refresh/FindLatestKey/FindKey/
// IsDateCached/GetTimePointsByDate all come from AwsNexradDataProvider),
// so provider_ is held as that common base -- only the actual download
// step differs per subclass (DownloadAndDecompress vs. DownloadRaw, since
// MRMS objects are gzipped and RTMA's/RRFS's aren't, see RtmaDataProvider's
// class comment), so that's the one place a fetch needs to know which
// concrete type it has.
std::shared_ptr<provider::AwsNexradDataProvider>
MakeProvider(map::GribCategory category, const ProductConfig& product)
{
   switch (category)
   {
   case map::GribCategory::Mrms:
      return std::make_shared<provider::MrmsDataProvider>(product.s3Product);
   case map::GribCategory::Rrfs:
   {
      auto rrfsProvider = std::make_shared<provider::RrfsDataProvider>();
      rrfsProvider->SetFileFamily(product.rrfsFileFamily);
      return rrfsProvider;
   }
   case map::GribCategory::Rtma:
   default:
      return std::make_shared<provider::RtmaDataProvider>();
   }
}

namespace
{

void EnsureDateListed(provider::AwsNexradDataProvider&      provider,
                      std::chrono::system_clock::time_point date)
{
   if (!provider.IsDateCached(date))
   {
      provider.GetTimePointsByDate(date, /* update */ true);
   }
}

// Mirrors the S3 key's own directory structure under the cache root
// (not flattened into one directory), and strips ".gz" since a cached
// file's bytes are always already-decompressed. Both matter for
// correctness, not just tidiness: decode_grib's ProductInfoFromPath (see
// grib-helper/src/decode_grib.cpp) derives the product label/valid time
// from the final path component's filename, expecting it to look exactly
// like the original S3 basename (no trailing ".gz", no directory
// components merged into it) -- and for RTMA specifically, the date only
// lives in the *directory* portion of the key (rtma2p5.<date>/...), not
// the basename, which repeats identically every day; flattening would
// have made every date collide on the same cache filename.
//
// Keyed by the S3 key alone (not by product), since the same downloaded
// bytes are reused across every RTMA field that shares one bundled file,
// and across every loop playthrough that revisits the same time.
std::string CachedDownloadPath(const std::string& key)
{
   std::string path = key;
   if (path.ends_with(".gz"))
   {
      path.resize(path.size() - 3);
   }
   return CacheDir() + "/" + path;
}

void PruneDownloadCache()
{
   namespace fs = std::filesystem;

   // Recursive: cached files live under per-product/per-date
   // subdirectories now (see CachedDownloadPath), not flat in CacheDir().
   std::error_code                  ec;
   std::vector<fs::directory_entry> entries;
   std::uintmax_t                   totalSize = 0;
   for (const auto& entry : fs::recursive_directory_iterator(CacheDir(), ec))
   {
      if (entry.is_regular_file())
      {
         entries.push_back(entry);
         totalSize += entry.file_size();
      }
   }
   if (ec)
   {
      return;
   }

   if (totalSize <= kMaxCacheSizeBytes_)
   {
      return;
   }

   std::sort(entries.begin(),
             entries.end(),
             [](const auto& a, const auto& b)
             { return a.last_write_time() < b.last_write_time(); });

   // Remove oldest-first until back under the cap, rather than a fixed
   // eviction count -- how many files that takes depends entirely on
   // which mix (many small MRMS frames vs. a few huge RRFS ones) is
   // actually over the line.
   for (const auto& entry : entries)
   {
      if (totalSize <= kMaxCacheSizeBytes_)
      {
         break;
      }

      const std::uintmax_t size = entry.file_size();
      fs::remove(entry.path(), ec);
      if (!ec)
      {
         totalSize -= size;
      }
   }
}

} // namespace

class GribManager::Impl
{
public:
   explicit Impl(map::GribCategory category) :
       category_ {category},
       // Held for the same reason GribProductLayer holds GribManager
       // itself: TimelineManager::Instance() only caches a weak_ptr, so a
       // discarded shared_ptr would let it be destroyed out from under our
       // signal connections.
       timelineManager_ {manager::TimelineManager::Instance()}
   {
      // Start with just the first entry active, matching this class's
      // pre-multi-select behavior exactly -- activeProducts_ must never
      // be empty (see SetProductActive), so this can't be done as a
      // default member initializer the way a single productIndex_ once
      // was; it needs the provider constructed alongside it.
      activeProducts_.insert(0);
      providers_[0] = MakeProvider(category_, Products(category_)[0]);
   }

   // Same idiom TimelineManager::Impl uses for its own thread pools:
   // explicitly stop+join here, in this destructor's *body*, before any
   // implicit member teardown begins -- fetchPool_'s posted lambdas
   // capture `this` (the outer GribManager) and touch this Impl's own
   // members, so they must fully finish before either starts being torn
   // down, not just before fetchPool_'s own destructor runs.
   ~Impl()
   {
      fetchPool_.stop();
      fetchPool_.join();
   }

   map::GribCategory category_;

   // Which products (indices into Products(category_)) are currently
   // fetched/decoded/rendered -- see SetProductActive(). A std::set (not
   // e.g. std::vector<bool>) both for the ordered "lowest = current/
   // primary" convention CurrentProductIndex() relies on, and because
   // insert/erase by index is simpler to reason about than an always-
   // full-size boolean vector. Always non-empty after construction.
   std::set<std::size_t> activeProducts_;

   // One provider per active product, not one shared provider -- needed
   // for correctness with MRMS (each product is a genuinely different S3
   // prefix/provider instance, see MrmsDataProvider's constructor), and
   // kept uniform for RTMA/RRFS too even though every product in either
   // of those categories would construct an identical provider (a few
   // redundant provider objects, not a redundant *download* -- see
   // QueueDownload's own comment on that).
   std::map<std::size_t, std::shared_ptr<provider::AwsNexradDataProvider>>
      providers_;

   std::shared_ptr<manager::TimelineManager> timelineManager_;
   QTimer*                                   timer_ {nullptr};

   // fetchMutex_ guards every field below it, since a background fetch
   // (see QueueDownload) reads/writes them from a fetchPool_ thread while
   // the GUI thread reads/writes them too (SetProductActive,
   // HandleSelectedTimeUpdated, etc.) -- unlike before RTMA/loop-prefetch
   // support, this manager is no longer single-threaded in practice.
   std::mutex fetchMutex_;
   std::map<std::size_t, std::string>
      lastKeys_; // currently displayed, per product
   std::map<std::size_t, std::string>
      lastRequestedKeys_; // most recently asked for, per product

   // Keyed by (productIndex, key) rather than just key -- see
   // QueueDownload's own comment for why two products sharing a category
   // (and so, often, a download) each still need their own in-flight
   // entry.
   std::set<std::pair<std::size_t, std::string>> inFlightKeys_;

   // Background download pool (2: enough to overlap a couple of prefetch
   // downloads without hammering S3/local bandwidth much harder than a
   // single live/archive fetch already did). Same boost::asio::thread_pool
   // + boost::asio::post pattern TimelineManager itself uses for its own
   // async work, not QtConcurrent/raw threads.
   boost::asio::thread_pool fetchPool_ {2};

   // Mirrors TimelineManager's own live/archive state (see
   // LiveStateUpdated/SelectedTimeUpdated) rather than querying it fresh
   // each time -- cheap, and matches how PlacefileLayer/AlertLayer cache
   // it (see timeline_manager.hpp).
   bool                                  isLive_ {true};
   std::chrono::system_clock::time_point selectedTime_ {};

   // RRFS-only (see SetRrfsCycle()/SetRrfsForecastHour() in the header) --
   // mirrors RrfsDataProvider::Impl's own three fields exactly, but kept
   // here too (not just read back from one provider) because this manager
   // can hold several independent RrfsDataProvider instances at once (one
   // per active product, see providers_ above) that all need to agree, and
   // because SetProductActive() can construct a brand new one at any time
   // (always at auto/latest/F000 defaults) that needs to be brought in
   // line with whatever selection is already in effect -- see
   // SyncRrfsProviderState(). Meaningless (left at defaults) for Mrms/Rtma.
   bool                                  rrfsUseLatestCycle_ {true};
   std::chrono::system_clock::time_point rrfsCycleOverride_ {};
   int                                   rrfsForecastHour_ {0};

   // Bounds PrefetchRrfsForecastHourRange() and (via GribDockWidget's own
   // Play/pause loop) where the animation wraps back to -- see
   // SetRrfsLoopRange()'s own doc. rrfsLoopEndHour_ < 0 means "unset, use
   // [0, MaxRrfsForecastHour()]" -- the original, unbounded behavior --
   // distinct from an explicit 0 upper bound.
   int rrfsLoopStartHour_ {0};
   int rrfsLoopEndHour_ {-1};
};

GribManager::GribManager(map::GribCategory category) :
    p(std::make_unique<Impl>(category))
{
   p->timer_ = new QTimer(this);
   connect(p->timer_, &QTimer::timeout, this, &GribManager::Poll);
   p->timer_->start(kPollIntervalMs_);

   connect(p->timelineManager_.get(),
           &manager::TimelineManager::LiveStateUpdated,
           this,
           &GribManager::HandleLiveStateUpdated);
   connect(p->timelineManager_.get(),
           &manager::TimelineManager::SelectedTimeUpdated,
           this,
           &GribManager::HandleSelectedTimeUpdated);
   connect(p->timelineManager_.get(),
           &manager::TimelineManager::AnimationStateUpdated,
           this,
           &GribManager::HandleAnimationStateUpdated);

   // Defer the first poll rather than doing it synchronously in the
   // constructor -- Poll() does blocking network I/O, and this manager is
   // constructed from GribProductLayer::Initialize(), which runs during
   // MapLibre's render setup.
   QTimer::singleShot(0, this, &GribManager::Poll);
}

GribManager::~GribManager() = default;

std::shared_ptr<GribManager> GribManager::Instance(map::GribCategory category)
{
   // One weak_ptr slot per category -- each category is an independent
   // singleton (independent fetch state, product table, frame files), not
   // one shared instance switching what it points to. A real bug lived
   // here until this map replaced it: only two slots existed
   // (mrmsInstance_/modelsInstance_) with a binary `== Mrms` check for
   // which to use, a leftover from when GribCategory only had two values
   // -- once it grew to three (Mrms/Rtma/Rrfs), Rtma and Rrfs silently
   // collapsed onto the same slot, meaning whichever category asked
   // first "won" and the other transparently got the same instance
   // (same products, same fetch state) instead of its own. Caught by
   // code review, not a test -- nothing exercised both categories'
   // Instance() calls closely enough together to notice.
   static std::map<map::GribCategory, std::weak_ptr<GribManager>> instances_;
   static std::mutex instanceMutex_ {};

   std::unique_lock lock(instanceMutex_);

   std::weak_ptr<GribManager>& slot = instances_[category];

   std::shared_ptr<GribManager> gribManager = slot.lock();

   if (gribManager == nullptr)
   {
      gribManager = std::make_shared<GribManager>(category);
      slot        = gribManager;
   }

   return gribManager;
}

std::vector<std::string> GribManager::ProductNames() const
{
   std::vector<std::string> names;
   const auto&              products = Products(p->category_);
   names.reserve(products.size());
   for (const auto& product : products)
   {
      names.push_back(product.displayName);
   }
   return names;
}

std::size_t GribManager::CurrentProductIndex() const
{
   // activeProducts_ is never empty after construction (see
   // SetProductActive) -- begin() is always valid.
   return *p->activeProducts_.begin();
}

std::string GribManager::CurrentProductName() const
{ return Products(p->category_)[CurrentProductIndex()].displayName; }

std::string GribManager::FormatValue(float rawValue) const
{
   const ProductConfig& product = Products(p->category_)[CurrentProductIndex()];
   auto&                unitSettings = settings::UnitSettings::Instance();

   switch (product.quantity)
   {
   case PhysicalQuantity::TemperatureKelvin:
   {
      const auto units = types::GetTemperatureUnitsFromName(
         unitSettings.temperature_units().GetValue());
      const float converted =
         types::ConvertTemperatureFromKelvin(rawValue, units);
      return fmt::format(
         "{:.2f} {}", converted, types::GetTemperatureUnitsAbbreviation(units));
   }
   case PhysicalQuantity::SpeedMetersPerSecond:
   {
      const auto units =
         types::GetSpeedUnitsFromName(unitSettings.speed_units().GetValue());
      return fmt::format("{:.2f} {}",
                         rawValue * types::GetSpeedUnitsScale(units),
                         types::GetSpeedUnitsAbbreviation(units));
   }
   case PhysicalQuantity::AccumulationMillimeters:
   {
      const auto units = types::GetAccumulationUnitsFromName(
         unitSettings.accumulation_units().GetValue());
      return fmt::format("{:.2f} {}",
                         rawValue * types::GetAccumulationUnitsScale(units),
                         types::GetAccumulationUnitsAbbreviation(units));
   }
   case PhysicalQuantity::PressurePascals:
   {
      const auto units = types::GetPressureUnitsFromName(
         unitSettings.pressure_units().GetValue());
      return fmt::format("{:.2f} {}",
                         rawValue * types::GetPressureUnitsScale(units),
                         types::GetPressureUnitsAbbreviation(units));
   }
   case PhysicalQuantity::None:
   default:
      return fmt::format("{:.2f} {}", rawValue, product.units);
   }
}

void GribManager::SetProductActive(const std::string& displayName, bool active)
{
   const auto& products = Products(p->category_);

   for (std::size_t i = 0; i < products.size(); ++i)
   {
      if (products[i].displayName != displayName)
      {
         continue;
      }

      const bool alreadyActive = p->activeProducts_.contains(i);
      if (active == alreadyActive)
      {
         return; // already in the requested state
      }

      if (active)
      {
         p->activeProducts_.insert(i);
         p->providers_[i] = MakeProvider(p->category_, products[i]);

         if (p->category_ == map::GribCategory::Rrfs)
         {
            // MakeProvider() always starts a fresh RrfsDataProvider at
            // auto/latest/F000 defaults -- bring it in line with whatever
            // cycle/hour selection is already in effect before the
            // Poll()/FetchArchiveFrame() call below runs against it.
            SyncRrfsProviderState(*p->providers_[i]);
         }
      }
      else
      {
         if (p->activeProducts_.size() == 1)
         {
            // Refused, not silently ignored -- CurrentProductIndex() (and
            // so GribProductLayer's rendering) needs at least one active
            // product to fall back on at all times.
            logger_->warn("Refusing to deactivate the only active product: {}",
                          displayName);
            return;
         }

         p->activeProducts_.erase(i);
         p->providers_.erase(i);
         {
            std::lock_guard lock(p->fetchMutex_);
            p->lastKeys_.erase(i);
            p->lastRequestedKeys_.erase(i);
         }
      }

      logger_->info(
         "Product {} now {}", displayName, active ? "active" : "inactive");

      if (active)
      {
         if (p->category_ == map::GribCategory::Rrfs)
         {
            // Rrfs's own cycle/hour selection (just synced above), not the
            // main timeline's live/selectedTime_ state -- see
            // SetRrfsCycle()'s own doc in grib_manager.hpp for why these
            // are independent axes.
            FetchRrfsSelection();
         }
         else if (p->isLive_)
         {
            Poll();
         }
         else
         {
            FetchArchiveFrame(p->selectedTime_);
         }
      }
      return;
   }

   logger_->warn("Unknown product: {}", displayName);
}

bool GribManager::IsProductActive(const std::string& displayName) const
{
   const auto& products = Products(p->category_);
   for (std::size_t i = 0; i < products.size(); ++i)
   {
      if (products[i].displayName == displayName)
      {
         return p->activeProducts_.contains(i);
      }
   }
   return false;
}

std::vector<std::string> GribManager::ActiveProductNames() const
{
   const auto&              products = Products(p->category_);
   std::vector<std::string> names;
   names.reserve(p->activeProducts_.size());
   for (auto index : p->activeProducts_)
   {
      names.push_back(products[index].displayName);
   }
   return names;
}

void GribManager::SyncRrfsProviderState(
   provider::AwsNexradDataProvider& provider) const
{
   auto& rrfsProvider = static_cast<provider::RrfsDataProvider&>(provider);

   if (p->rrfsUseLatestCycle_)
   {
      rrfsProvider.UseLatestCycle();
   }
   else
   {
      rrfsProvider.SetCycle(p->rrfsCycleOverride_);
   }
   rrfsProvider.SetForecastHour(p->rrfsForecastHour_);
}

void GribManager::FetchRrfsSelection()
{
   using namespace std::chrono;

   const std::size_t index = CurrentProductIndex();
   auto&             rrfsProvider =
      static_cast<provider::RrfsDataProvider&>(*p->providers_.at(index));

   const auto time =
      rrfsProvider.CurrentCycle() + hours {rrfsProvider.ForecastHour()};

   FetchArchiveFrame(time);
}

void GribManager::SetRrfsCycle(std::chrono::system_clock::time_point cycleTime)
{
   if (p->category_ != map::GribCategory::Rrfs)
   {
      logger_->warn("SetRrfsCycle() is only meaningful for GribCategory::Rrfs");
      return;
   }

   p->rrfsUseLatestCycle_ = false;
   p->rrfsCycleOverride_  = cycleTime;

   for (auto& [index, providerPtr] : p->providers_)
   {
      SyncRrfsProviderState(*providerPtr);
   }

   FetchRrfsSelection();
}

void GribManager::UseLatestRrfsCycle()
{
   if (p->category_ != map::GribCategory::Rrfs)
   {
      logger_->warn(
         "UseLatestRrfsCycle() is only meaningful for GribCategory::Rrfs");
      return;
   }

   p->rrfsUseLatestCycle_ = true;

   for (auto& [index, providerPtr] : p->providers_)
   {
      SyncRrfsProviderState(*providerPtr);
   }

   FetchRrfsSelection();
}

bool GribManager::IsUsingLatestRrfsCycle() const
{ return p->category_ != map::GribCategory::Rrfs || p->rrfsUseLatestCycle_; }

std::chrono::system_clock::time_point GribManager::CurrentRrfsCycle() const
{
   if (p->category_ != map::GribCategory::Rrfs)
   {
      return {};
   }

   return static_cast<provider::RrfsDataProvider&>(
             *p->providers_.at(CurrentProductIndex()))
      .CurrentCycle();
}

void GribManager::SetRrfsForecastHour(int hour)
{
   if (p->category_ != map::GribCategory::Rrfs)
   {
      logger_->warn(
         "SetRrfsForecastHour() is only meaningful for GribCategory::Rrfs");
      return;
   }

   p->rrfsForecastHour_ = hour;

   for (auto& [index, providerPtr] : p->providers_)
   {
      SyncRrfsProviderState(*providerPtr);
   }

   FetchRrfsSelection();
}

int GribManager::RrfsForecastHour() const
{ return (p->category_ == map::GribCategory::Rrfs) ? p->rrfsForecastHour_ : 0; }

int GribManager::MaxRrfsForecastHour() const
{
   if (p->category_ != map::GribCategory::Rrfs)
   {
      return 0;
   }

   return provider::RrfsDataProvider::MaxForecastHourForCycle(
      CurrentRrfsCycle());
}

void GribManager::SetRrfsLoopRange(int startHour, int endHour)
{
   if (p->category_ != map::GribCategory::Rrfs)
   {
      logger_->warn(
         "SetRrfsLoopRange() is only meaningful for GribCategory::Rrfs");
      return;
   }

   p->rrfsLoopStartHour_ = startHour;
   p->rrfsLoopEndHour_   = endHour;
}

int GribManager::RrfsLoopStartHour() const
{
   return (p->category_ == map::GribCategory::Rrfs) ? p->rrfsLoopStartHour_ : 0;
}

int GribManager::RrfsLoopEndHour() const
{ return (p->category_ == map::GribCategory::Rrfs) ? p->rrfsLoopEndHour_ : -1; }

void GribManager::PrefetchRrfsForecastHourRange()
{
   if (p->category_ != map::GribCategory::Rrfs)
   {
      logger_->warn(
         "PrefetchRrfsForecastHourRange() is only meaningful for "
         "GribCategory::Rrfs");
      return;
   }

   const std::size_t index  = CurrentProductIndex();
   const auto        cycle  = CurrentRrfsCycle();
   const auto        family = Products(p->category_)[index].rrfsFileFamily;
   const int         maxHour =
      provider::RrfsDataProvider::MaxForecastHourForCycle(cycle);

   const int startHour = std::clamp(p->rrfsLoopStartHour_, 0, maxHour);
   const int endHour   = (p->rrfsLoopEndHour_ < 0) ?
                            maxHour :
                            std::clamp(p->rrfsLoopEndHour_, startHour, maxHour);

   logger_->debug("Prefetching {} RRFS forecast hour(s) for cycle {} ({}-{}h)",
                  endHour - startHour + 1,
                  scwx::util::TimeString(cycle),
                  startHour,
                  endHour);

   for (int hour = startHour; hour <= endHour; ++hour)
   {
      QueueDownload(index,
                    provider::RrfsDataProvider::BuildKey(cycle, hour, family));
   }
}

void GribManager::Poll()
{
   // Only the live path polls for "whatever's newest" -- once the user
   // scrubs into archive mode, HandleSelectedTimeUpdated() drives fetches
   // instead, and there's nothing meaningful for a "latest" poll to do.
   if (!p->isLive_)
   {
      return;
   }

   // Copied rather than iterated live: SetProductActive can run on this
   // same (GUI) thread re-entrantly if a fetch below somehow triggered
   // one synchronously, and mutating activeProducts_ mid-range-for would
   // be undefined behavior either way.
   const std::set<std::size_t> activeProducts = p->activeProducts_;

   for (auto index : activeProducts)
   {
      if (Products(p->category_)[index].derivedIndex == "ship")
      {
         // SHIP's own two-file fetch/decode is handled entirely through
         // FetchArchiveFrameForProduct()'s own dedicated dispatch (see
         // its doc) -- this generic "find whatever's latest, RequestFrame
         // it" loop would call decode_grib with the wrong (single-input)
         // CLI form for SHIP's two-input mode.
         continue;
      }

      auto& provider = p->providers_.at(index);

      auto [newObjects, totalObjects] = provider->Refresh();
      logger_->debug("Refresh (product {}): {} new / {} total objects",
                     index,
                     newObjects,
                     totalObjects);

      const std::string latestKey = provider->FindLatestKey();

      std::string currentKey;
      {
         std::lock_guard lock(p->fetchMutex_);
         currentKey = p->lastKeys_[index];
      }

      if (latestKey.empty() || latestKey == currentKey)
      {
         continue;
      }

      logger_->info("New GRIB file for product {}: {}", index, latestKey);
      RequestFrame(index, latestKey);
   }
}

void GribManager::HandleLiveStateUpdated(bool isLive)
{
   p->isLive_ = isLive;

   if (isLive)
   {
      // Snap back to "latest" immediately rather than waiting for the
      // next 4-minute tick.
      Poll();
   }
}

void GribManager::HandleSelectedTimeUpdated(
   std::chrono::system_clock::time_point dateTime)
{
   p->selectedTime_ = dateTime;

   if (!p->isLive_)
   {
      FetchArchiveFrame(dateTime);
   }
}

void GribManager::HandleAnimationStateUpdated(types::AnimationState state)
{
   // Deliberately not gated on !p->isLive_: TimelineManager::Impl::Play()
   // emits AnimationStateUpdated synchronously, before PlaySync() (posted
   // to its own thread pool) ever calls SelectTime() -- the call that
   // actually flips isLive_ false via LiveStateUpdated. Gating on isLive_
   // here would race and likely lose when the user presses Play directly
   // from live view, which is exactly the common case. GetLoopStartAndEnd
   // Times() is well-defined either way (falls back to "now" as the end
   // time when still live), so there's nothing to gate on -- Play always
   // means "about to step through a bounded range."
   if (state == types::AnimationState::Play)
   {
      PrefetchLoopRange();
   }
}

void GribManager::PrefetchLoopRange()
{
   using namespace std::chrono;

   // Current product only, not every active one -- see this method's own
   // doc in grib_manager.hpp. A reasonable follow-up, not attempted here.
   const std::size_t index    = CurrentProductIndex();
   auto&             provider = p->providers_.at(index);

   auto [startTime, endTime] = p->timelineManager_->GetLoopStartAndEndTimes();
   if (startTime >= endTime)
   {
      return;
   }

   for (auto date = floor<days>(startTime); date <= floor<days>(endTime);
        date += days {1})
   {
      EnsureDateListed(*provider, date);
   }

   // 1-minute ticks match PlaySync's own per-step advance (see
   // TimelineManager::Impl::PlaySync) -- fine enough that FindKey's
   // nearest-match won't skip a real file that lands between ticks, while
   // naturally deduping to the actual (much sparser) set of real files via
   // the std::set below.
   std::set<std::string> neededKeys;
   for (auto t = startTime; t <= endTime; t += minutes {1})
   {
      std::string key = provider->FindKey(t);
      if (!key.empty())
      {
         neededKeys.insert(key);
      }
   }

   logger_->debug("Prefetching {} frame(s) for loop range {} to {}",
                  neededKeys.size(),
                  scwx::util::TimeString(startTime),
                  scwx::util::TimeString(endTime));

   for (const auto& key : neededKeys)
   {
      QueueDownload(index, key);
   }
}

bool GribManager::FetchArchiveFrame(std::chrono::system_clock::time_point time)
{
   // Copied for the same re-entrancy reason as Poll()'s own copy.
   const std::set<std::size_t> activeProducts = p->activeProducts_;

   bool anyRequested = false;
   for (auto index : activeProducts)
   {
      anyRequested |= FetchArchiveFrameForProduct(index, time);
   }
   return anyRequested;
}

bool GribManager::FetchArchiveFrameForProduct(
   std::size_t productIndex, std::chrono::system_clock::time_point time)
{
   using namespace std::chrono;

   auto& provider = p->providers_.at(productIndex);

   if (Products(p->category_)[productIndex].derivedIndex == "ship")
   {
      // SHIP's own two-file dispatch -- see QueueShipInput()/
      // ApplyShipIfReady()'s own docs. Never reaches RequestFrame()/
      // QueueDownload()/ApplyCachedDownload() below, all of which assume
      // one key per product.
      FetchShipSelection(productIndex);
      return true;
   }

   std::string key;

   if (p->category_ == map::GribCategory::Rrfs)
   {
      // A real correctness gap, found and fixed while adding the prslev
      // file family (see [[supercell-wx-wpc-qpf]] session's own RRFS
      // follow-up): FindKey()/EnsureDateListed() below rely on the base
      // class's day-granularity object cache, which is exactly right for
      // MRMS/RTMA (one listing covers every time within that whole day)
      // but wrong for RRFS -- its own GetPrefix() resolves one *exact*
      // file per cycle/forecast-hour/file-family selection, not a whole
      // day's worth. Confirmed live: GetTimePointsByDate()'s own inner
      // "has this day ever been listed" check ignores its own `update`
      // argument once true, for *any* prior selection that day -- so
      // there is no way to force a correct re-list through that API once
      // the day has been seen once, and changing forecast hour twice on
      // the same calendar day silently kept returning the *first* hour's
      // key both times. RrfsDataProvider::BuildKey() -- already used by
      // PrefetchRrfsForecastHourRange() for the same underlying reason --
      // sidesteps the whole listing path: deterministic, so resolving
      // which file this selection wants needs no network call at all
      // (only downloading it does, and RequestFrame()/QueueDownload()
      // already handle a file that turns out not to exist gracefully).
      auto& rrfsProvider = static_cast<provider::RrfsDataProvider&>(*provider);
      const auto family  = Products(p->category_)[productIndex].rrfsFileFamily;
      key                = provider::RrfsDataProvider::BuildKey(
         rrfsProvider.CurrentCycle(), rrfsProvider.ForecastHour(), family);
   }
   else
   {
      // MRMS/RTMA's S3 listing is per-UTC-day, so list the day `time`
      // falls on before searching it.
      const auto date = floor<days>(time);
      EnsureDateListed(*provider, date);

      key = provider->FindKey(time);

      // A selection near midnight UTC can have its nearest real file on
      // the adjacent day's listing rather than the day `time` itself
      // falls on -- try both neighbors before giving up.
      if (key.empty())
      {
         EnsureDateListed(*provider, date - days {1});
         key = provider->FindKey(time);
      }
      if (key.empty())
      {
         EnsureDateListed(*provider, date + days {1});
         key = provider->FindKey(time);
      }
   }

   if (key.empty())
   {
      // Nothing found nearby (e.g. archive time predates the source, or a
      // network hiccup) -- leave whatever frame is currently showing
      // rather than clearing it out from under the user for a likely
      // transient gap.
      logger_->warn("No GRIB file found near {} for product {}",
                    scwx::util::TimeString(time),
                    productIndex);
      return false;
   }

   {
      std::lock_guard lock(p->fetchMutex_);
      if (key == p->lastKeys_[productIndex])
      {
         // Already showing this one -- archive scrubbing/playback can
         // re-fire this handler rapidly, don't redo the request each time.
         return true;
      }
   }

   logger_->info("Archive GRIB file for {} (product {}): {}",
                 scwx::util::TimeString(time),
                 productIndex,
                 key);
   RequestFrame(productIndex, key);
   return true;
}

// SHIP's own two-file selection: resolves both of its current inputs'
// keys (deterministically, via BuildKey() -- same reasoning as the
// normal RRFS branch above), kicks off a background download for
// whichever isn't already cached, and checks (synchronously, cheap) in
// case both already are. Never touches RequestFrame()/QueueDownload()/
// ApplyCachedDownload() -- those all assume one key decodes one
// product's frame, which doesn't hold for a two-input derived index.
void GribManager::FetchShipSelection(std::size_t productIndex)
{
   auto& rrfsProvider =
      static_cast<provider::RrfsDataProvider&>(*p->providers_.at(productIndex));

   const auto key2dfld =
      provider::RrfsDataProvider::BuildKey(rrfsProvider.CurrentCycle(),
                                           rrfsProvider.ForecastHour(),
                                           provider::RrfsFileFamily::TwoDField);
   const auto keyPrslev = provider::RrfsDataProvider::BuildKey(
      rrfsProvider.CurrentCycle(),
      rrfsProvider.ForecastHour(),
      provider::RrfsFileFamily::PressureLevel);

   {
      std::lock_guard lock(p->fetchMutex_);
      p->lastRequestedKeys_[productIndex] = key2dfld + "|" + keyPrslev;
   }

   if (!std::filesystem::exists(CachedDownloadPath(key2dfld)))
   {
      QueueShipInput(productIndex, key2dfld);
   }
   if (!std::filesystem::exists(CachedDownloadPath(keyPrslev)))
   {
      QueueShipInput(productIndex, keyPrslev);
   }

   // Covers the case both were already cached (a synchronous decode,
   // same "show when ready" idea as RequestFrame()'s own already-cached
   // branch) -- if either was just queued above, this call's own "both
   // exist yet?" check below will correctly say no for now, and each
   // QueueShipInput() job re-checks this same way once its own download
   // finishes.
   ApplyShipIfReady(productIndex);
}

// Called once after queuing (in case both inputs were already cached)
// and again from each QueueShipInput() job's own completion -- checks
// whether SHIP's *current* selection's two inputs are both on disk yet
// and, if so, decodes. Recomputes both keys fresh from the provider's
// own live cycle/forecast-hour state rather than trusting whatever
// triggered this call, so a stale completion (the selection moved on
// while a download was in flight) naturally finds the *new* combination
// still incomplete and does nothing, rather than applying an outdated
// pair.
void GribManager::ApplyShipIfReady(std::size_t productIndex)
{
   auto& rrfsProvider =
      static_cast<provider::RrfsDataProvider&>(*p->providers_.at(productIndex));

   const auto key2dfld =
      provider::RrfsDataProvider::BuildKey(rrfsProvider.CurrentCycle(),
                                           rrfsProvider.ForecastHour(),
                                           provider::RrfsFileFamily::TwoDField);
   const auto keyPrslev = provider::RrfsDataProvider::BuildKey(
      rrfsProvider.CurrentCycle(),
      rrfsProvider.ForecastHour(),
      provider::RrfsFileFamily::PressureLevel);

   if (!std::filesystem::exists(CachedDownloadPath(key2dfld)) ||
       !std::filesystem::exists(CachedDownloadPath(keyPrslev)))
   {
      return; // not both ready yet
   }

   const std::string combinedKey = key2dfld + "|" + keyPrslev;

   {
      std::lock_guard lock(p->fetchMutex_);
      if (combinedKey == p->lastKeys_[productIndex])
      {
         return; // already showing this exact combination
      }
      if (combinedKey != p->lastRequestedKeys_[productIndex])
      {
         return; // superseded by a newer selection since this was queued
      }
   }

   ApplyShipDownload(productIndex, key2dfld, keyPrslev);
}

// Downloads one of SHIP's two inputs (if not already in flight) on the
// background thread pool, then re-checks ApplyShipIfReady() once done --
// mirrors QueueDownload()'s own shape (in-flight dedup, background pool,
// PruneDownloadCache) but doesn't call it directly: its own completion
// unconditionally invokes ApplyCachedDownload() with *one* key, which
// would run decode_grib with the wrong (single-input) CLI form for
// SHIP's two-input mode.
void GribManager::QueueShipInput(std::size_t        productIndex,
                                 const std::string& key)
{
   {
      std::lock_guard lock(p->fetchMutex_);
      if (!p->inFlightKeys_.insert({productIndex, key}).second)
      {
         return; // already downloading this specific input
      }
   }

   std::shared_ptr<provider::AwsNexradDataProvider> provider =
      p->providers_.at(productIndex);
   auto statusManager = manager::StatusManager::Instance();

   boost::asio::post(
      p->fetchPool_,
      [this, productIndex, key, provider, statusManager]()
      {
         const std::string cachedPath = CachedDownloadPath(key);
         std::filesystem::create_directories(
            std::filesystem::path(cachedPath).parent_path());

         // One status entry per (productIndex, key) pair -- SHIP's two
         // inputs can genuinely be downloading at once, each wanting its
         // own progress entry, same reasoning as QueueDownload()'s own
         // per-(category,productIndex) id.
         const std::string statusId =
            fmt::format("grib-ship-{}-{}", productIndex, key);
         const auto progressCallback =
            [&statusManager, &statusId](std::int64_t bytesReceived,
                                        std::int64_t totalBytes)
         {
            statusManager->ReportProgress(
               statusId, "SHIP", bytesReceived, totalBytes);
         };

         auto& rrfsProvider =
            static_cast<provider::RrfsDataProvider&>(*provider);
         auto downloaded =
            rrfsProvider.DownloadRaw(key, cachedPath, progressCallback);
         statusManager->ReportComplete(statusId);

         {
            std::lock_guard lock(p->fetchMutex_);
            p->inFlightKeys_.erase({productIndex, key});
         }

         if (!downloaded.has_value())
         {
            logger_->warn("Failed to download SHIP input {}", key);
            return;
         }

         PruneDownloadCache();

         ApplyShipIfReady(productIndex);
      });
}

// Decodes SHIP from its two already-downloaded inputs and atomically
// replaces productIndex's own frame file -- ApplyCachedDownload()'s own
// two-input counterpart; kept separate rather than extending that
// function's signature, since every other product only ever has one
// input.
bool GribManager::ApplyShipDownload(std::size_t        productIndex,
                                    const std::string& key2dfld,
                                    const std::string& keyPrslev)
{
   const ProductConfig& product = Products(p->category_)[productIndex];
   const std::string    tmpFramePath =
      map::GetGribFramePath(p->category_, productIndex) + ".tmp";

   QStringList decodeArgs;
   decodeArgs << "--derived" << "ship"
              << QString::fromStdString(CachedDownloadPath(key2dfld))
              << QString::fromStdString(CachedDownloadPath(keyPrslev))
              << QString::fromStdString(tmpFramePath)
              << QString::number(product.colorOffset)
              << QString::number(product.colorScale)
              << QString::number(product.noDataThreshold);
   if (product.contourInterval > 0.0f)
   {
      decodeArgs << QString::number(product.contourInterval);
   }

   QProcess decodeProcess;
   decodeProcess.start(QString::fromStdString(DecodeGribPath()), decodeArgs);

   if (!decodeProcess.waitForFinished(10000) || decodeProcess.exitCode() != 0)
   {
      logger_->warn("decode_grib failed for SHIP: {}",
                    decodeProcess.readAllStandardError().toStdString());
      std::filesystem::remove(tmpFramePath);
      return false;
   }

   std::error_code ec;
   std::filesystem::rename(
      tmpFramePath, map::GetGribFramePath(p->category_, productIndex), ec);

   if (ec)
   {
      logger_->warn("Could not replace frame file: {}", ec.message());
      std::filesystem::remove(tmpFramePath);
      return false;
   }

   {
      std::lock_guard lock(p->fetchMutex_);
      p->lastKeys_[productIndex] = key2dfld + "|" + keyPrslev;
   }

   logger_->info("Updated {}",
                 map::GetGribFramePath(p->category_, productIndex));
   Q_EMIT FrameReady(productIndex);
   return true;
}

void GribManager::RequestFrame(std::size_t productIndex, const std::string& key)
{
   {
      std::lock_guard lock(p->fetchMutex_);
      p->lastRequestedKeys_[productIndex] = key;
   }

   const ProductConfig& product = Products(p->category_)[productIndex];

   if (std::filesystem::exists(CachedDownloadPath(key)))
   {
      // Already on disk (a prior fetch, or a prefetch that's since
      // completed) -- decode_grib's own runtime is cheap (~30ms), so just
      // do it synchronously right here rather than bouncing to the
      // background pool for no real benefit. This is the "show when
      // ready" path: by the time playback actually reaches a prefetched
      // time, this is normally all that runs.
      ApplyCachedDownload(productIndex,
                          key,
                          product.shortName,
                          product.colorOffset,
                          product.colorScale,
                          product.noDataThreshold,
                          product.contourInterval,
                          product.derivedIndex,
                          product.typeOfLevel,
                          product.topLevel,
                          product.bottomLevel,
                          product.startStep,
                          product.lengthOfTimeRange);
      return;
   }

   QueueDownload(productIndex, key);
}

void GribManager::QueueDownload(std::size_t        productIndex,
                                const std::string& key)
{
   {
      std::lock_guard lock(p->fetchMutex_);
      if (!p->inFlightKeys_.insert({productIndex, key}).second)
      {
         return; // already downloading for this product (a prefetch and a
                 // direct request can race for the same key; only one
                 // should fetch it) -- see this method's own doc in
                 // grib_manager.hpp for why the dedup key includes
                 // productIndex rather than being just `key`.
      }
   }

   // Snapshot everything this job needs by value -- it may run well after
   // the user has deactivated this product or moved on, so it must not
   // read p->providers_/p->activeProducts_ live from the pool thread.
   std::shared_ptr<provider::AwsNexradDataProvider> provider =
      p->providers_.at(productIndex);
   const ProductConfig product = Products(p->category_)[productIndex];

   // Held by value in the download lambda below, not looked up fresh via
   // Instance() there -- keeps the singleton alive for this download's
   // full duration regardless of whether anything else (e.g. a status
   // bar widget) already holds a longer-lived reference to it yet.
   auto statusManager = manager::StatusManager::Instance();

   boost::asio::post(
      p->fetchPool_,
      [this, productIndex, key, provider, product, statusManager]()
      {
         const std::string cachedPath = CachedDownloadPath(key);
         std::filesystem::create_directories(
            std::filesystem::path(cachedPath).parent_path());

         // Keyed by (category_, productIndex) -- distinct GribManager
         // instances/products can genuinely download at once (see this
         // class's own multi-select support), each wanting its own status
         // entry rather than clobbering another's.
         const std::string statusId = fmt::format(
            "grib-{}-{}", static_cast<int>(p->category_), productIndex);
         const auto progressCallback =
            [&statusManager, &statusId, &product](std::int64_t bytesReceived,
                                                  std::int64_t totalBytes)
         {
            statusManager->ReportProgress(
               statusId, product.displayName, bytesReceived, totalBytes);
         };

         // MRMS objects are gzipped, RTMA's/RRFS's aren't (see
         // RtmaDataProvider's class comment) -- provider is held as the
         // common AwsNexradDataProvider base, so this is the one place
         // that needs the concrete type back. category_ (unlike
         // providers_/activeProducts_) never changes after construction,
         // so reading it live via `this` from the pool thread is safe.
         std::optional<std::string> downloaded;
         switch (p->category_)
         {
         case map::GribCategory::Mrms:
            downloaded =
               static_cast<provider::MrmsDataProvider&>(*provider)
                  .DownloadAndDecompress(key, cachedPath, progressCallback);
            break;
         case map::GribCategory::Rrfs:
            downloaded =
               static_cast<provider::RrfsDataProvider&>(*provider).DownloadRaw(
                  key, cachedPath, progressCallback);
            break;
         case map::GribCategory::Rtma:
         default:
            downloaded =
               static_cast<provider::RtmaDataProvider&>(*provider).DownloadRaw(
                  key, cachedPath, progressCallback);
            break;
         }

         statusManager->ReportComplete(statusId);

         {
            std::lock_guard lock(p->fetchMutex_);
            p->inFlightKeys_.erase({productIndex, key});
         }

         if (!downloaded.has_value())
         {
            logger_->warn("Failed to download {}", key);
            return;
         }

         PruneDownloadCache();

         // Only apply this to the display if it's still what's wanted --
         // otherwise playback/scrubbing has moved on since this was
         // queued, and applying it now would show a stale frame. It stays
         // cached on disk regardless, for a loop repeat or a scrub back.
         bool stillWanted;
         {
            std::lock_guard lock(p->fetchMutex_);
            stillWanted = (p->lastRequestedKeys_[productIndex] == key);
         }

         if (stillWanted)
         {
            ApplyCachedDownload(productIndex,
                                key,
                                product.shortName,
                                product.colorOffset,
                                product.colorScale,
                                product.noDataThreshold,
                                product.contourInterval,
                                product.derivedIndex,
                                product.typeOfLevel,
                                product.topLevel,
                                product.bottomLevel,
                                product.startStep,
                                product.lengthOfTimeRange);
         }
      });
}

bool GribManager::ApplyCachedDownload(std::size_t        productIndex,
                                      const std::string& key,
                                      const std::string& shortName,
                                      float              colorOffset,
                                      float              colorScale,
                                      float              noDataThreshold,
                                      float              contourInterval,
                                      const std::string& derivedIndex,
                                      const std::string& typeOfLevel,
                                      long               topLevel,
                                      long               bottomLevel,
                                      long               startStep,
                                      long               lengthOfTimeRange)
{
   const std::string cachedPath = CachedDownloadPath(key);
   const std::string tmpFramePath =
      map::GetGribFramePath(p->category_, productIndex) + ".tmp";

   // decode_grib's own runtime is ~25-30ms for a normal single-message
   // decode (measured against a full CONUS MRMS file); a derived index
   // like STP reads several messages and does real per-pixel math, but
   // still comfortably sub-second against a CONUS grid -- fine either
   // way to block whichever thread calls this (the GUI thread for an
   // already-cached hit, a fetchPool_ thread otherwise).
   QStringList decodeArgs;
   if (!derivedIndex.empty())
   {
      // decode_grib's `--derived <name>` mode has its own fixed field
      // list per index name -- shortName is meaningless here and left
      // out entirely (see ProductConfig::derivedIndex).
      decodeArgs << "--derived" << QString::fromStdString(derivedIndex);
   }
   decodeArgs << QString::fromStdString(cachedPath)
              << QString::fromStdString(tmpFramePath)
              << QString::number(colorOffset) << QString::number(colorScale)
              << QString::number(noDataThreshold);
   if (!shortName.empty())
   {
      // Selects one message out of RTMA's 13-field bundled file (see
      // decode_grib's shortName argument) -- MRMS's single-message files
      // never pass this.
      decodeArgs << QString::fromStdString(shortName);

      // decode_grib's CLI requires shortName before contourInterval
      // before typeOfLevel/topLevel/bottomLevel before startStep/
      // lengthOfTimeRange (see its own usage string) -- so a product
      // needing any qualifier always emits contourInterval too, even
      // when it's 0 (fill mode), and a product needing startStep/
      // lengthOfTimeRange also emits typeOfLevel/topLevel/bottomLevel
      // even when it doesn't need *those* (RRFS's two "tp" messages
      // share identical level metadata -- see ProductConfig's own doc),
      // since the CLI is strictly positional.
      if (startStep >= 0 || lengthOfTimeRange >= 0)
      {
         decodeArgs << QString::number(contourInterval)
                    << QString::fromStdString(typeOfLevel)
                    << QString::number(topLevel) << QString::number(bottomLevel)
                    << QString::number(startStep)
                    << QString::number(lengthOfTimeRange);
      }
      else if (!typeOfLevel.empty())
      {
         decodeArgs << QString::number(contourInterval)
                    << QString::fromStdString(typeOfLevel)
                    << QString::number(topLevel)
                    << QString::number(bottomLevel);
      }
      else if (contourInterval > 0.0f)
      {
         decodeArgs << QString::number(contourInterval);
      }
   }
   else if (!derivedIndex.empty() && contourInterval > 0.0f)
   {
      // The --derived form also accepts a trailing contourInterval, just
      // without a shortName ahead of it (see decode_grib's RunDerived
      // usage string) -- no current derived index actually sets one, but
      // wiring it through now avoids a second special case later.
      decodeArgs << QString::number(contourInterval);
   }

   QProcess decodeProcess;
   decodeProcess.start(QString::fromStdString(DecodeGribPath()), decodeArgs);

   if (!decodeProcess.waitForFinished(10000) || decodeProcess.exitCode() != 0)
   {
      logger_->warn("decode_grib failed for {}: {}",
                    cachedPath,
                    decodeProcess.readAllStandardError().toStdString());
      std::filesystem::remove(tmpFramePath);
      return false;
   }

   std::error_code ec;
   std::filesystem::rename(
      tmpFramePath, map::GetGribFramePath(p->category_, productIndex), ec);

   if (ec)
   {
      logger_->warn("Could not replace frame file: {}", ec.message());
      std::filesystem::remove(tmpFramePath);
      return false;
   }

   {
      std::lock_guard lock(p->fetchMutex_);
      p->lastKeys_[productIndex] = key;
   }

   logger_->info("Updated {}",
                 map::GetGribFramePath(p->category_, productIndex));
   Q_EMIT FrameReady(productIndex);
   return true;
}

} // namespace scwx::qt::manager
