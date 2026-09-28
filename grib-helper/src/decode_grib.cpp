// Decodes a GRIB2 field with eccodes and emits the binary-framed wire
// format used to hand grid data to Supercell Wx (JSON metadata line, then
// raw float32 grid bytes -- no PNG, no per-value text encoding). Invoked
// out-of-process by manager::GribManager and manager::WindBarbManager
// (see scwx-qt/source/scwx/qt/manager/grib_manager.cpp), not linked
// directly into wxdata: eccodes isn't on conancenter, so keeping this a
// separate executable avoids needing a from-source eccodes build as part
// of the main dependency graph.

#include <eccodes.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{

// GRIB2's Grid Definition Section is self-describing (eccodes exposes it as
// the "gridType" string key) -- so rather than hardcoding "MRMS is
// regular_ll, RTMA is lambert" by source, decode_grib reads it per message
// and branches. Confirmed via grib_ls against real files: MRMS reports
// "regular_ll", RTMA (and most other NCEP CONUS-nest models -- HRRR, RRFS,
// RAP, NAM -- for later sources) reports "lambert". Add more GridType
// values/branches here as new sources need them.
enum class GridType
{
   RegularLatLon,
   Lambert
};

struct GridInfo
{
   GridType type {GridType::RegularLatLon};
   long     ni {};
   long     nj {};
   double   lat1 {}; // first grid point; NW corner for regular_ll, but for
                     // lambert this is whatever corner the grid actually
                     // scans from (see iScansNegatively/jScansPositively --
                     // RTMA scans from the SW corner, confirmed via grib_ls)
   double lon1 {};   // normalized to -180..180
   double missingValue {};

   // RegularLatLon only.
   double di {};
   double dj {};

   // Lambert only -- see Snyder, "Map Projections: A Working Manual",
   // sections 15-1 to 15-4 (forward) / 15-8 to 15-11 (inverse), which take
   // exactly this parameter set.
   double lov {};    // orientation longitude (central meridian), degrees
   double lad {};    // latitude of the cone's tangency, degrees
   double latin1 {}; // first standard parallel, degrees
   double latin2 {}; // second standard parallel, degrees
   double dx {};     // grid spacing, metres
   double dy {};     // grid spacing, metres
   double radius {}; // sphere radius, metres -- read from the message
                     // rather than hardcoding NCEP's usual 6371200: it's
                     // right there (shapeOfTheEarth=1, confirmed via
                     // grib_ls against the real RTMA file), so there's no
                     // reason for the renderer to assume it independently
                     // and risk silently drifting out of sync.
};

double NormalizeLongitude(double lon)
{
   if (lon > 180.0)
   {
      return lon - 360.0;
   }
   return lon;
}

GridInfo ReadGridInfo(codes_handle* h)
{
   GridInfo info {};

   CODES_CHECK(codes_get_long(h, "Ni", &info.ni), nullptr);
   CODES_CHECK(codes_get_long(h, "Nj", &info.nj), nullptr);
   CODES_CHECK(
      codes_get_double(h, "latitudeOfFirstGridPointInDegrees", &info.lat1),
      nullptr);
   CODES_CHECK(
      codes_get_double(h, "longitudeOfFirstGridPointInDegrees", &info.lon1),
      nullptr);
   CODES_CHECK(codes_get_double(h, "missingValue", &info.missingValue),
               nullptr);
   info.lon1 = NormalizeLongitude(info.lon1);

   char   gridTypeBuf[32];
   size_t gridTypeLen = sizeof(gridTypeBuf);
   CODES_CHECK(codes_get_string(h, "gridType", gridTypeBuf, &gridTypeLen),
               nullptr);
   const std::string gridType(gridTypeBuf);

   if (gridType == "lambert")
   {
      info.type = GridType::Lambert;
      CODES_CHECK(codes_get_double(h, "LoVInDegrees", &info.lov), nullptr);
      CODES_CHECK(codes_get_double(h, "LaDInDegrees", &info.lad), nullptr);
      CODES_CHECK(codes_get_double(h, "Latin1InDegrees", &info.latin1),
                  nullptr);
      CODES_CHECK(codes_get_double(h, "Latin2InDegrees", &info.latin2),
                  nullptr);
      CODES_CHECK(codes_get_double(h, "DxInMetres", &info.dx), nullptr);
      CODES_CHECK(codes_get_double(h, "DyInMetres", &info.dy), nullptr);
      CODES_CHECK(codes_get_double(h, "radius", &info.radius), nullptr);
      info.lov = NormalizeLongitude(info.lov);
   }
   else if (gridType == "regular_ll")
   {
      info.type = GridType::RegularLatLon;
      CODES_CHECK(codes_get_double(h, "iDirectionIncrementInDegrees", &info.di),
                  nullptr);
      CODES_CHECK(codes_get_double(h, "jDirectionIncrementInDegrees", &info.dj),
                  nullptr);
   }
   else
   {
      throw std::runtime_error("Unsupported gridType: " + gridType);
   }

   return info;
}

std::vector<double> ReadValues(codes_handle* h)
{
   size_t count = 0;
   CODES_CHECK(codes_get_size(h, "values", &count), nullptr);

   std::vector<double> values(count);
   CODES_CHECK(codes_get_double_array(h, "values", values.data(), &count),
               nullptr);
   values.resize(count);

   return values;
}

struct ProductInfo
{
   std::string product;   // e.g. "MergedReflectivityQCComposite_00.50"
   std::string validTime; // ISO8601 UTC, e.g. "2026-09-19T03:58:38Z"
};

// RTMA bundles 13 fields into one file (unlike MRMS, one S3 object per
// field), so decode_grib needs to pick a specific message out of it.
// Returns the matching handle (caller owns it, must codes_handle_delete)
// or nullptr if not found; every non-matching handle opened along the
// way is deleted here, not leaked to the caller.
//
// shortName alone used to be enough for every caller (RTMA's shortName
// is clean and meaningful, e.g. "2t", "10si", unlike MRMS's "unknown"
// shortName/parameterName) -- optional typeOfLevel/topLevel/bottomLevel
// filters were added once several real RRFS fields turned out NOT to be
// unique by shortName alone (confirmed via a real grib_ls dump, not
// assumed): cape/cin/hlcy/vucsh/vvcsh/tcc all appear more than once in
// the same 2dfld file at different levels/layers sharing one shortName
// -- e.g. surface CAPE and three different mixed-layer-CAPE depths all
// report shortName "cape". An empty `typeOfLevel` (default) or a
// negative `topLevel`/`bottomLevel` (default) means "don't care about
// this" -- most products (every RTMA field, MRMS's single-message
// files, MSLET/Lightning Threat) really are unambiguous by shortName
// alone and pass none of the optional filters.
//
// `startStep`/`lengthOfTimeRange` (same "-1 means don't care" default)
// exist for a different kind of ambiguity `typeOfLevel`/`topLevel`/
// `bottomLevel` can't resolve: RRFS's 2dfld file carries *two* `tp`
// (Total Precipitation) messages sharing the exact same surface/level-0
// metadata -- one is the last hour's own accumulation (`stepRange`
// "<h-1>-<h>", `lengthOfTimeRange` always 1 regardless of forecast
// hour), the other is accumulated since the run's own start
// (`stepRange` "0-<h>", `startStep` always 0). `lengthOfTimeRange`
// disambiguates the former (a fixed, hour-independent value); `startStep`
// disambiguates the latter -- confirmed live via grib_ls against a real
// forecast-hour-5 2dfld file, not assumed from the GRIB2 spec alone.
codes_handle* FindMessage(FILE*              f,
                          const std::string& shortName,
                          const std::string& typeOfLevel       = "",
                          long               topLevel          = -1,
                          long               bottomLevel       = -1,
                          long               startStep         = -1,
                          long               lengthOfTimeRange = -1)
{
   int           err = 0;
   codes_handle* h   = nullptr;

   while ((h = codes_grib_handle_new_from_file(nullptr, f, &err)) != nullptr)
   {
      if (err != 0)
      {
         codes_handle_delete(h);
         throw std::runtime_error(
            std::string("codes_grib_handle_new_from_file failed: ") +
            codes_get_error_message(err));
      }

      char   nameBuf[64];
      size_t nameLen = sizeof(nameBuf);
      CODES_CHECK(codes_get_string(h, "shortName", nameBuf, &nameLen), nullptr);

      bool matches = (shortName == nameBuf);

      if (matches && !typeOfLevel.empty())
      {
         char   levelTypeBuf[64];
         size_t levelTypeLen = sizeof(levelTypeBuf);
         CODES_CHECK(
            codes_get_string(h, "typeOfLevel", levelTypeBuf, &levelTypeLen),
            nullptr);
         matches = (typeOfLevel == levelTypeBuf);
      }

      if (matches && (topLevel >= 0 || bottomLevel >= 0))
      {
         long top    = -1;
         long bottom = -1;
         codes_get_long(h, "topLevel", &top);
         codes_get_long(h, "bottomLevel", &bottom);
         matches = (topLevel < 0 || top == topLevel) &&
                   (bottomLevel < 0 || bottom == bottomLevel);
      }

      if (matches && startStep >= 0)
      {
         long actualStartStep = -1;
         codes_get_long(h, "startStep", &actualStartStep);
         matches = (actualStartStep == startStep);
      }

      if (matches && lengthOfTimeRange >= 0)
      {
         long actualLengthOfTimeRange = -1;
         codes_get_long(h, "lengthOfTimeRange", &actualLengthOfTimeRange);
         matches = (actualLengthOfTimeRange == lengthOfTimeRange);
      }

      if (matches)
      {
         return h;
      }

      codes_handle_delete(h);
   }

   return nullptr;
}

// RTMA's filename (e.g. "rtma2p5.t00z.2dvaranl_ndfd.grb2_wexp") carries no
// per-field name or timestamp the way MRMS's does -- valid time and the
// field label both come from the matched message's own metadata instead.
// dataDate/dataTime are reliable here (unlike the MRMS README's noted
// truncation concern, RTMA is always on the hour, so minute/second
// precision was never in question).
ProductInfo ProductInfoFromHandle(codes_handle* h, const std::string& shortName)
{
   long dataDate = 0;
   long dataTime = 0;
   CODES_CHECK(codes_get_long(h, "dataDate", &dataDate), nullptr);
   CODES_CHECK(codes_get_long(h, "dataTime", &dataTime), nullptr);

   char dateBuf[9];
   std::snprintf(dateBuf, sizeof(dateBuf), "%08ld", dataDate);
   char timeBuf[5];
   std::snprintf(timeBuf, sizeof(timeBuf), "%04ld", dataTime);

   const std::string ymd(dateBuf);
   const std::string hm(timeBuf);

   ProductInfo info;
   info.product   = shortName;
   info.validTime = ymd.substr(0, 4) + "-" + ymd.substr(4, 2) + "-" +
                    ymd.substr(6, 2) + "T" + hm.substr(0, 2) + ":" +
                    hm.substr(2, 2) + ":00Z";
   return info;
}

// eccodes has no usable product name for MRMS's local GRIB2 table
// (shortName/parameterName both come back as "unknown"/junk -- confirmed
// against a real file), and dataTime truncates seconds. The MRMS filename
// itself carries both cleanly, e.g.
// "MRMS_MergedReflectivityQCComposite_00.50_20201016-053435.grib2" -- so
// derive both from the filename instead of fighting eccodes. Split into
// two fields (not one display string) specifically so the valid time is
// real, usable data -- e.g. for a future staleness check against
// TimelineManager's selected time -- not just text baked into a label.
ProductInfo ProductInfoFromPath(const std::string& inputPath)
{
   std::string stem = std::filesystem::path(inputPath).stem().string();

   constexpr std::string_view kPrefix = "MRMS_";
   if (stem.starts_with(kPrefix))
   {
      stem = stem.substr(kPrefix.size());
   }

   // Trailing "_<YYYYMMDD>-<HHMMSS>" is the valid time; everything before
   // it is the product (name + level, e.g. "..._00.50").
   static const std::regex kTimeRegex {R"(_(\d{8})-(\d{6})$)"};
   std::smatch             match;

   ProductInfo info;
   if (std::regex_search(stem, match, kTimeRegex))
   {
      info.product = stem.substr(0, match.position(0));

      const std::string& ymd = match[1].str();
      const std::string& hms = match[2].str();
      info.validTime         = ymd.substr(0, 4) + "-" + ymd.substr(4, 2) + "-" +
                               ymd.substr(6, 2) + "T" + hms.substr(0, 2) + ":" +
                               hms.substr(2, 2) + ":" + hms.substr(4, 2) + "Z";
   }
   else
   {
      // Fall back to the whole stem as the product, no valid time --
      // still decodable, just without a parsed timestamp.
      info.product = stem;
   }

   return info;
}

// Minimal escaping for a JSON string value -- sufficient for filenames,
// not a general-purpose JSON encoder.
std::string JsonEscape(const std::string& s)
{
   std::string out;
   out.reserve(s.size());
   for (char c : s)
   {
      if (c == '"' || c == '\\')
      {
         out += '\\';
      }
      out += c;
   }
   return out;
}

// Deliberately hand-rolled, not a JSON library: this header is a fixed set
// of known scalar fields, one line, machine-generated on one side and
// parsed by a matching hand-rolled reader on the other (read_frame.cpp).
// A real implementation would use the same JSON facility the rest of
// Supercell Wx already links (e.g. its existing JSON dependency), not a
// bespoke parser -- this is scoped to prove the framing shape.
// Colorizing range for the field this file holds -- GribManager knows this
// per MRMS product (reflectivity vs. rotation track vs. hail vs. precip
// all have wildly different physical ranges and "no coverage" sentinels),
// decode_grib just embeds whatever it's told so GribProductLayer can
// recolor correctly when the user switches products at runtime, without
// decode_grib itself needing a product catalog.
struct ColorRange
{
   float offset;
   float scale;
   float noDataThreshold;

   // 0 means "fill mode" (the normal palette-LUT rendering); a nonzero
   // value tells GribProductLayer's shader to draw anti-aliased isolines
   // at every multiple of this value instead (e.g. 400 Pa / 4 hPa for
   // MSLP), the same "decode_grib just embeds whatever it's told" idea
   // as offset/scale/noDataThreshold above -- GribManager's ProductConfig
   // is still the one place that decides which products get this.
   float contourInterval = 0.0f;
};

std::string BuildHeaderLine(const GridInfo&    grid,
                            const ProductInfo& productInfo,
                            const ColorRange&  colorRange,
                            size_t             byteLength)
{
   // Lambert fields are always emitted (zero-filled when gridType is
   // regular_ll, and vice versa for di/dj) rather than conditionally
   // omitted -- keeps this a fixed-shape line like the rest of the header,
   // consistent with how colorOffset/colorScale/noDataThreshold were added
   // earlier (reader-side backward compat is ExtractNumberOr's job, not
   // this writer's).
   char buf[768];
   std::snprintf(buf,
                 sizeof(buf),
                 R"({"type":"grid","dtype":"float32","nx":%ld,"ny":%ld,)"
                 R"("gridType":"%s","lat1":%.6f,"lon1":%.6f,)"
                 R"("di":%.6f,"dj":%.6f,)"
                 R"("lov":%.6f,"lad":%.6f,"latin1":%.6f,"latin2":%.6f,)"
                 R"("dx":%.6f,"dy":%.6f,"radius":%.6f,)"
                 R"("missingValue":%.6f,"product":"%s","validTime":"%s",)"
                 R"("colorOffset":%.6f,"colorScale":%.6f,)"
                 R"("noDataThreshold":%.6f,"contourInterval":%.6f,)"
                 R"("byteLength":%zu})",
                 grid.ni,
                 grid.nj,
                 grid.type == GridType::Lambert ? "lambert" : "regular_ll",
                 grid.lat1,
                 grid.lon1,
                 grid.di,
                 grid.dj,
                 grid.lov,
                 grid.lad,
                 grid.latin1,
                 grid.latin2,
                 grid.dx,
                 grid.dy,
                 grid.radius,
                 grid.missingValue,
                 JsonEscape(productInfo.product).c_str(),
                 JsonEscape(productInfo.validTime).c_str(),
                 colorRange.offset,
                 colorRange.scale,
                 colorRange.noDataThreshold,
                 colorRange.contourInterval,
                 byteLength);
   return std::string(buf) + "\n";
}

// Writes the binary-framed wire format and prints the same decode
// summary as the normal single-message path -- shared with
// RunDerived() below so a derived index's output is written identically
// to any other frame, not a second copy of this logic. `missingValue`
// is passed explicitly rather than read off `grid` since a derived
// index's own output sentinel isn't necessarily any one of its input
// messages' own missingValue (see ComputeStp).
bool WriteFrame(const std::string&        outputPath,
                const GridInfo&           grid,
                const ProductInfo&        productInfo,
                const ColorRange&         colorRange,
                const std::vector<float>& wireValues,
                double                    missingValue)
{
   double minVal       = std::numeric_limits<double>::infinity();
   double maxVal       = -std::numeric_limits<double>::infinity();
   double sum          = 0.0;
   size_t missingCount = 0;

   for (float v : wireValues)
   {
      if (static_cast<double>(v) == missingValue)
      {
         ++missingCount;
         continue;
      }

      minVal = std::min(minVal, static_cast<double>(v));
      maxVal = std::max(maxVal, static_cast<double>(v));
      sum += v;
   }

   const size_t validCount = wireValues.size() - missingCount;
   const double meanVal    = validCount > 0 ? sum / validCount : 0.0;
   const size_t byteLength = wireValues.size() * sizeof(float);

   std::ofstream out(outputPath, std::ios::binary);
   if (!out)
   {
      std::cerr << "Could not open " << outputPath << " for writing\n";
      return false;
   }

   const std::string header =
      BuildHeaderLine(grid, productInfo, colorRange, byteLength);
   out.write(header.data(), static_cast<std::streamsize>(header.size()));
   out.write(reinterpret_cast<const char*>(wireValues.data()),
             static_cast<std::streamsize>(byteLength));
   out.close();

   std::cout << "Decoded " << productInfo.product << "\n"
             << "  Valid:    " << productInfo.validTime << "\n"
             << "  Grid:     " << grid.ni << " x " << grid.nj << " ("
             << (grid.ni * grid.nj) << " cells)\n"
             << "  Origin:   lat1=" << grid.lat1 << " lon1=" << grid.lon1
             << "\n"
             << "  Spacing:  di=" << grid.di << " dj=" << grid.dj << "\n"
             << "  Missing:  " << missingCount << " / " << wireValues.size()
             << " cells\n"
             << "  Range:    [" << minVal << ", " << maxVal
             << "], mean=" << meanVal << "\n"
             << "  Frame:    " << outputPath << " (" << header.size()
             << " byte header + " << byteLength << " byte payload)\n";

   return true;
}

// One derived-index input field: the message's decoded grid values plus
// its own missingValue sentinel (each eccodes message can define its
// own, not assumed shared across fields in the same file) so ComputeStp
// can propagate "missing here" pointwise from any input into the
// output, rather than only detecting a whole-grid failure.
struct DerivedField
{
   std::vector<double> values;
   double              missingValue;
};

DerivedField ReadDerivedField(FILE*              f,
                              const std::string& shortName,
                              const std::string& typeOfLevel,
                              long               topLevel,
                              long               bottomLevel,
                              long               expectedNi,
                              long               expectedNj)
{
   std::rewind(f);
   codes_handle* h =
      FindMessage(f, shortName, typeOfLevel, topLevel, bottomLevel);
   if (h == nullptr)
   {
      throw std::runtime_error("Derived-index input not found: shortName=" +
                               shortName + " typeOfLevel=" + typeOfLevel);
   }

   long ni = 0;
   long nj = 0;
   CODES_CHECK(codes_get_long(h, "Ni", &ni), nullptr);
   CODES_CHECK(codes_get_long(h, "Nj", &nj), nullptr);
   if (ni != expectedNi || nj != expectedNj)
   {
      codes_handle_delete(h);
      throw std::runtime_error("Derived-index input grid size mismatch: " +
                               shortName);
   }

   DerivedField field;
   CODES_CHECK(codes_get_double(h, "missingValue", &field.missingValue),
               nullptr);
   field.values = ReadValues(h);
   codes_handle_delete(h);
   return field;
}

// STP (Significant Tornado Parameter), fixed-layer form -- Thompson et
// al. 2003, SPC's own documented predecessor to today's effective-layer
// default form (see spc.noaa.gov/exper/mesoanalysis/help/help_stor.html).
// The effective-layer form needs ESRH/EBWD/effective-inflow-base, which
// need a full vertical-profile parcel test RRFS doesn't ship as ready
// fields -- substantially bigger scope, deliberately not attempted here.
//
//   STP = (SBCAPE/1500) * LCLterm * (SRH1/150) * SHRterm * CINterm
//
//   LCLterm = clip((2000 - LCL_AGL) / 1000, 0.0, 1.0)
//   SHRterm = 0            if SHEAR6 < 12.5 m/s
//           = SHEAR6 / 20  if 12.5 <= SHEAR6 <= 30 m/s
//           = 1.5          if SHEAR6 > 30 m/s
//   CINterm = clip((200 + SBCIN) / 150, 0.0, 1.0)
//
// All 7 inputs come from the same 2dfld file already being fetched for
// every other RRFS product -- no prslev dependency (unlike SHIP, which
// needs T700/T500 and stays deferred until prslev support exists).
// Field selection (shortName/typeOfLevel/topLevel/bottomLevel per input)
// confirmed against a real downloaded 2dfld file, not carried over
// untested from a reference implementation in a different project (see
// wxqt's own docs/derived-severe-indices-plan.md, whose SHIP/STP work
// this leans on for the formula itself and its live field-mapping
// findings -- but not its exact field labels blindly: this session's
// own re-check against a real file found LCL height ships as shortName
// "gh" here, not "hgt" as that project's doc states, so re-verifying
// mattered).
struct StpResult
{
   std::vector<float> values;
   double             missingValue = -9999.0;
};

StpResult ComputeStp(FILE* f, long ni, long nj)
{
   const auto terrain = ReadDerivedField(f, "orog", "surface", 0, 0, ni, nj);
   const auto sbcape  = ReadDerivedField(f, "cape", "surface", 0, 0, ni, nj);
   const auto sbcin   = ReadDerivedField(f, "cin", "surface", 0, 0, ni, nj);
   const auto srh1 =
      ReadDerivedField(f, "hlcy", "heightAboveGroundLayer", 1000, 0, ni, nj);
   const auto lclGh =
      ReadDerivedField(f, "gh", "adiabaticCondensation", 0, 0, ni, nj);
   const auto shear6u =
      ReadDerivedField(f, "vucsh", "heightAboveGroundLayer", 0, 6000, ni, nj);
   const auto shear6v =
      ReadDerivedField(f, "vvcsh", "heightAboveGroundLayer", 0, 6000, ni, nj);

   StpResult    result;
   const size_t count = static_cast<size_t>(ni * nj);
   result.values.resize(count);

   for (size_t i = 0; i < count; ++i)
   {
      if (terrain.values[i] == terrain.missingValue ||
          sbcape.values[i] == sbcape.missingValue ||
          sbcin.values[i] == sbcin.missingValue ||
          srh1.values[i] == srh1.missingValue ||
          lclGh.values[i] == lclGh.missingValue ||
          shear6u.values[i] == shear6u.missingValue ||
          shear6v.values[i] == shear6v.missingValue)
      {
         result.values[i] = static_cast<float>(result.missingValue);
         continue;
      }

      const double lclAgl  = lclGh.values[i] - terrain.values[i];
      const double lclTerm = std::clamp((2000.0 - lclAgl) / 1000.0, 0.0, 1.0);

      const double shear6 = std::sqrt(shear6u.values[i] * shear6u.values[i] +
                                      shear6v.values[i] * shear6v.values[i]);
      double       shrTerm;
      if (shear6 < 12.5)
      {
         shrTerm = 0.0;
      }
      else if (shear6 > 30.0)
      {
         shrTerm = 1.5;
      }
      else
      {
         shrTerm = shear6 / 20.0;
      }

      const double cinTerm =
         std::clamp((200.0 + sbcin.values[i]) / 150.0, 0.0, 1.0);

      const double stp = (sbcape.values[i] / 1500.0) * lclTerm *
                         (srh1.values[i] / 150.0) * shrTerm * cinTerm;

      result.values[i] = static_cast<float>(stp);
   }

   return result;
}

// SHIP (Significant Hail Parameter), SPC mesoanalysis form. Formula and
// caps verified against wxqt's own docs/derived-severe-indices-plan.md
// (itself a port from a working Python/MetPy prototype, cross-checked
// there against SPC's own published definition) -- but every *field*
// re-verified against a real downloaded RRFS file here, not trusted
// blind, same discipline as ComputeStp's own "gh not hgt" catch above:
// confirmed live that MU-parcel CAPE is "cape"/pressureFromGroundLayer/
// 180-0mb (the same field an existing product already calls MUCAPE for
// exactly this reason) and that 2m specific humidity ("2sh") exists
// natively -- used here as this formula's "most-unstable parcel mixing
// ratio," a common practical simplification when a true MU-parcel-level
// moisture profile isn't available (2m humidity directly, matching wxqt's
// own precedent of using the 2m field rather than a derived value).
//
//   SHIP = (MUCAPE * MIXR * LR75 * (-T500) * SHEAR6) / 42,000,000
//
// Capped *before* multiplying:
//   SHEAR6 (0-6km bulk shear, m/s) clamped to [7, 27]
//   MIXR (g/kg) clamped to [11, 13.6]
//   T500 (C) capped at -5.5 (warmer -> -5.5, colder untouched)
// Rescaled *after* multiplying:
//   MUCAPE < 1300 J/kg -> SHIP *= MUCAPE/1300
//   LR75 < 5.8 C/km    -> SHIP *= LR75/5.8
//   FZL_AGL < 2400 m   -> SHIP *= FZL_AGL/2400
//
// LR75 (700-500mb lapse rate) is computed as a direct height difference
// from prslev's own HGT700/HGT500 (both fetched alongside T700/T500
// anyway) rather than the hypsometric-equation approximation wxqt's own
// prototype used from mean temperature -- simpler and more accurate once
// both heights are already on hand. Unlike every other derived index
// here, SHIP's inputs span *two* separate downloaded files (2dfld +
// prslev) -- see RunDerived's own two-input handling below.
struct ShipResult
{
   std::vector<float> values;
   double             missingValue = -9999.0;
};

ShipResult ComputeShip(FILE* f2dfld, FILE* fPrslev, long ni, long nj)
{
   const auto terrain =
      ReadDerivedField(f2dfld, "orog", "surface", 0, 0, ni, nj);
   const auto mucape = ReadDerivedField(
      f2dfld, "cape", "pressureFromGroundLayer", 18000, 0, ni, nj);
   const auto spfh2 =
      ReadDerivedField(f2dfld, "2sh", "heightAboveGround", 2, 2, ni, nj);
   const auto shear6u = ReadDerivedField(
      f2dfld, "vucsh", "heightAboveGroundLayer", 0, 6000, ni, nj);
   const auto shear6v = ReadDerivedField(
      f2dfld, "vvcsh", "heightAboveGroundLayer", 0, 6000, ni, nj);
   const auto fzlGh =
      ReadDerivedField(f2dfld, "gh", "isothermZero", 0, 0, ni, nj);

   const auto t500 =
      ReadDerivedField(fPrslev, "t", "isobaricInhPa", 500, 500, ni, nj);
   const auto t700 =
      ReadDerivedField(fPrslev, "t", "isobaricInhPa", 700, 700, ni, nj);
   const auto gh500 =
      ReadDerivedField(fPrslev, "gh", "isobaricInhPa", 500, 500, ni, nj);
   const auto gh700 =
      ReadDerivedField(fPrslev, "gh", "isobaricInhPa", 700, 700, ni, nj);

   ShipResult   result;
   const size_t count = static_cast<size_t>(ni * nj);
   result.values.resize(count);

   for (size_t i = 0; i < count; ++i)
   {
      if (terrain.values[i] == terrain.missingValue ||
          mucape.values[i] == mucape.missingValue ||
          spfh2.values[i] == spfh2.missingValue ||
          shear6u.values[i] == shear6u.missingValue ||
          shear6v.values[i] == shear6v.missingValue ||
          fzlGh.values[i] == fzlGh.missingValue ||
          t500.values[i] == t500.missingValue ||
          t700.values[i] == t700.missingValue ||
          gh500.values[i] == gh500.missingValue ||
          gh700.values[i] == gh700.missingValue)
      {
         result.values[i] = static_cast<float>(result.missingValue);
         continue;
      }

      // Specific humidity (kg/kg, GRIB2's own SI unit) -> mixing ratio
      // (g/kg): w = q/(1-q).
      const double mixr = std::clamp(
         (spfh2.values[i] / (1.0 - spfh2.values[i])) * 1000.0, 11.0, 13.6);

      const double t500C = std::min(t500.values[i] - 273.15, -5.5);

      const double lr75 = (t700.values[i] - t500.values[i]) /
                          ((gh500.values[i] - gh700.values[i]) / 1000.0);

      const double shear6 =
         std::clamp(std::sqrt(shear6u.values[i] * shear6u.values[i] +
                              shear6v.values[i] * shear6v.values[i]),
                    7.0,
                    27.0);

      const double fzlAgl = fzlGh.values[i] - terrain.values[i];

      double ship =
         (mucape.values[i] * mixr * lr75 * (-t500C) * shear6) / 42000000.0;

      if (mucape.values[i] < 1300.0)
      {
         ship *= mucape.values[i] / 1300.0;
      }
      if (lr75 < 5.8)
      {
         ship *= lr75 / 5.8;
      }
      if (fzlAgl < 2400.0)
      {
         ship *= fzlAgl / 2400.0;
      }

      result.values[i] = static_cast<float>(ship);
   }

   return result;
}

// Shared by "shear6" and "wind10" below -- both are just
// sqrt(u^2 + v^2) of a wind-component pair, differing only in which two
// messages they read. Missing propagates the same pointwise way
// ComputeStp's inputs do.
struct VectorMagnitudeResult
{
   std::vector<float> values;
   double             missingValue = -9999.0;
};

VectorMagnitudeResult ComputeVectorMagnitude(FILE*              f,
                                             long               ni,
                                             long               nj,
                                             const std::string& uShortName,
                                             const std::string& uTypeOfLevel,
                                             long               uTopLevel,
                                             long               uBottomLevel,
                                             const std::string& vShortName,
                                             const std::string& vTypeOfLevel,
                                             long               vTopLevel,
                                             long               vBottomLevel)
{
   const auto u = ReadDerivedField(
      f, uShortName, uTypeOfLevel, uTopLevel, uBottomLevel, ni, nj);
   const auto v = ReadDerivedField(
      f, vShortName, vTypeOfLevel, vTopLevel, vBottomLevel, ni, nj);

   VectorMagnitudeResult result;
   const size_t          count = static_cast<size_t>(ni * nj);
   result.values.resize(count);

   for (size_t i = 0; i < count; ++i)
   {
      if (u.values[i] == u.missingValue || v.values[i] == v.missingValue)
      {
         result.values[i] = static_cast<float>(result.missingValue);
         continue;
      }

      result.values[i] = static_cast<float>(
         std::sqrt(u.values[i] * u.values[i] + v.values[i] * v.values[i]));
   }

   return result;
}

// Dispatches a computed/composite index instead of decoding a single
// GRIB message -- separate argv layout from the normal single-message
// form (no shortName: derived indices have their own fixed internal
// field list, not a caller-supplied one): `decode_grib --derived <name>
// <input> <output> [colorOffset colorScale noDataThreshold
// [contourInterval]]`. SHIP is the one exception -- its inputs span two
// separate files (2dfld + prslev, see ComputeShip's own doc), so it gets
// a second positional `<input2>` between `<input>` and `<output>`
// instead: `--derived ship <input2dfld> <inputPrslev> <output> [...]`.
int RunDerived(int argc, char** argv)
{
   const std::string name      = (argc >= 3) ? argv[2] : std::string();
   const bool        twoInputs = (name == "ship");

   if ((!twoInputs && argc != 5 && argc != 8 && argc != 9) ||
       (twoInputs && argc != 6 && argc != 9 && argc != 10))
   {
      std::cerr
         << "Usage: " << argv[0]
         << " --derived <name> <input.grib2> <output.frame> "
            "[colorOffset colorScale noDataThreshold [contourInterval]]\n"
         << "       " << argv[0]
         << " --derived ship <input2dfld.grib2> <inputPrslev.grib2> "
            "<output.frame> [colorOffset colorScale noDataThreshold "
            "[contourInterval]]\n";
      return 1;
   }

   const char* inputPath  = argv[3];
   const char* inputPath2 = twoInputs ? argv[4] : nullptr;
   const char* outputPath = argv[twoInputs ? 5 : 4];

   if (name != "stp" && name != "shear6" && name != "wind10" &&
       name != "wind500" && name != "ship")
   {
      std::cerr << "Unknown derived index: " << name << "\n";
      return 1;
   }

   ColorRange colorRange {0.0f, 1.0f, -9000.0f};
   const int  colorArgvStart = twoInputs ? 6 : 5;
   if (argc == colorArgvStart + 3 || argc == colorArgvStart + 4)
   {
      colorRange.offset = std::strtof(argv[colorArgvStart], nullptr);
      colorRange.scale  = std::strtof(argv[colorArgvStart + 1], nullptr);
      colorRange.noDataThreshold =
         std::strtof(argv[colorArgvStart + 2], nullptr);
   }
   if (argc == colorArgvStart + 4)
   {
      colorRange.contourInterval =
         std::strtof(argv[colorArgvStart + 3], nullptr);
   }

   FILE* f = std::fopen(inputPath, "rb");
   if (f == nullptr)
   {
      std::cerr << "Could not open " << inputPath << "\n";
      return 1;
   }

   FILE* f2 = nullptr;
   if (twoInputs)
   {
      f2 = std::fopen(inputPath2, "rb");
      if (f2 == nullptr)
      {
         std::fclose(f);
         std::cerr << "Could not open " << inputPath2 << "\n";
         return 1;
      }
   }

   try
   {
      // Grid geometry and valid time come from whichever message is read
      // first, one of this index's own inputs -- every derived index's
      // inputs share the same native RRFS grid (all from the same 2dfld
      // file's 3km Lambert grid) and the same run's valid time;
      // ReadDerivedField itself checks every subsequent field against
      // this same (ni, nj) rather than trusting that.
      std::string firstShortName   = "orog";
      std::string firstTypeOfLevel = "surface";
      long        firstTopLevel    = 0;
      long        firstBottomLevel = 0;
      if (name == "shear6")
      {
         firstShortName   = "vucsh";
         firstTypeOfLevel = "heightAboveGroundLayer";
         firstTopLevel    = 0;
         firstBottomLevel = 6000;
      }
      else if (name == "wind10")
      {
         firstShortName   = "10u";
         firstTypeOfLevel = "";
         firstTopLevel    = -1;
         firstBottomLevel = -1;
      }
      else if (name == "wind500")
      {
         // prslev, not 2dfld -- eccodes' own shortName for isobaric u/v
         // is just "u"/"v" (confirmed live via grib_ls against a real
         // prslev file, not assumed to match 2dfld's "10u"/"10v"
         // convention), disambiguated the same way cape/cin/etc. already
         // are in 2dfld: topLevel==bottomLevel==level for a single
         // isobaric level (also confirmed live), so the existing
         // typeOfLevel/topLevel/bottomLevel filter needs no extension.
         firstShortName   = "u";
         firstTypeOfLevel = "isobaricInhPa";
         firstTopLevel    = 500;
         firstBottomLevel = 500;
      }

      std::rewind(f);
      codes_handle* firstHandle = FindMessage(
         f, firstShortName, firstTypeOfLevel, firstTopLevel, firstBottomLevel);
      if (firstHandle == nullptr)
      {
         std::fclose(f);
         if (f2 != nullptr)
         {
            std::fclose(f2);
         }
         std::cerr << "Could not find " << firstShortName << " for " << name
                   << "\n";
         return 1;
      }

      const GridInfo grid = ReadGridInfo(firstHandle);
      long           ni   = 0;
      long           nj   = 0;
      CODES_CHECK(codes_get_long(firstHandle, "Ni", &ni), nullptr);
      CODES_CHECK(codes_get_long(firstHandle, "Nj", &nj), nullptr);
      ProductInfo productInfo = ProductInfoFromHandle(firstHandle, name);
      codes_handle_delete(firstHandle);
      productInfo.product = name;

      std::vector<float> values;
      double             missingValue = -9999.0;

      if (name == "stp")
      {
         const StpResult stp = ComputeStp(f, ni, nj);
         values              = std::move(stp.values);
         missingValue        = stp.missingValue;
      }
      else if (name == "shear6")
      {
         const auto mag = ComputeVectorMagnitude(f,
                                                 ni,
                                                 nj,
                                                 "vucsh",
                                                 "heightAboveGroundLayer",
                                                 0,
                                                 6000,
                                                 "vvcsh",
                                                 "heightAboveGroundLayer",
                                                 0,
                                                 6000);
         values         = std::move(mag.values);
         missingValue   = mag.missingValue;
      }
      else if (name == "wind10")
      {
         const auto mag = ComputeVectorMagnitude(
            f, ni, nj, "10u", "", -1, -1, "10v", "", -1, -1);
         values       = std::move(mag.values);
         missingValue = mag.missingValue;
      }
      else if (name == "wind500")
      {
         const auto mag = ComputeVectorMagnitude(f,
                                                 ni,
                                                 nj,
                                                 "u",
                                                 "isobaricInhPa",
                                                 500,
                                                 500,
                                                 "v",
                                                 "isobaricInhPa",
                                                 500,
                                                 500);
         values         = std::move(mag.values);
         missingValue   = mag.missingValue;
      }
      else // "ship"
      {
         const ShipResult ship = ComputeShip(f, f2, ni, nj);
         values                = std::move(ship.values);
         missingValue          = ship.missingValue;
      }

      std::fclose(f);
      if (f2 != nullptr)
      {
         std::fclose(f2);
      }

      if (!WriteFrame(
             outputPath, grid, productInfo, colorRange, values, missingValue))
      {
         return 1;
      }
   }
   catch (const std::exception& e)
   {
      std::fclose(f);
      if (f2 != nullptr)
      {
         std::fclose(f2);
      }
      std::cerr << "Error: " << e.what() << "\n";
      return 1;
   }

   return 0;
}

} // namespace

int main(int argc, char** argv)
{
   if (argc >= 2 && std::string_view(argv[1]) == "--derived")
   {
      return RunDerived(argc, argv);
   }

   if (argc != 3 && argc != 6 && argc != 7 && argc != 8 && argc != 11 &&
       argc != 13)
   {
      std::cerr << "Usage: " << argv[0]
                << " <input.grib2> <output.frame> "
                   "[colorOffset colorScale noDataThreshold [shortName "
                   "[contourInterval [typeOfLevel topLevel bottomLevel "
                   "[startStep lengthOfTimeRange]]]]]\n"
                << "       " << argv[0]
                << " --derived <name> <input.grib2> <output.frame> "
                   "[colorOffset colorScale noDataThreshold "
                   "[contourInterval]]\n";
      return 1;
   }

   const char* inputPath  = argv[1];
   const char* outputPath = argv[2];

   // Defaults match the real base-reflectivity palette's range (see
   // res/palettes/wct/DR.pal in the supercell-wx tree: -20 to 75 dBZ) --
   // callers that don't care about other products (e.g. read_frame's own
   // tests, or running this by hand) get the same behavior as before
   // per-product ranges existed.
   ColorRange  colorRange {-20.0f, 95.0f, 0.0f};
   std::string shortName; // empty => single-message file (e.g. MRMS)
   // "Don't care" defaults matching FindMessage's own -- most products
   // are unambiguous by shortName alone and never reach argc==11/13 (see
   // that function's own comment for the real fields that aren't:
   // cape/cin/hlcy/vucsh/vvcsh/tcc/rare all share a shortName with
   // another level/layer in the same RRFS 2dfld file; RRFS's two `tp`
   // messages share level metadata too but need startStep/
   // lengthOfTimeRange instead, hence argc==13).
   std::string typeOfLevel;
   long        topLevel          = -1;
   long        bottomLevel       = -1;
   long        startStep         = -1;
   long        lengthOfTimeRange = -1;
   if (argc == 6 || argc == 7 || argc == 8 || argc == 11 || argc == 13)
   {
      colorRange.offset          = std::strtof(argv[3], nullptr);
      colorRange.scale           = std::strtof(argv[4], nullptr);
      colorRange.noDataThreshold = std::strtof(argv[5], nullptr);
   }
   if (argc == 7 || argc == 8 || argc == 11 || argc == 13)
   {
      shortName = argv[6];
   }
   if (argc == 8 || argc == 11 || argc == 13)
   {
      colorRange.contourInterval = std::strtof(argv[7], nullptr);
   }
   if (argc == 11 || argc == 13)
   {
      typeOfLevel = argv[8];
      topLevel    = std::strtol(argv[9], nullptr, 10);
      bottomLevel = std::strtol(argv[10], nullptr, 10);
   }
   if (argc == 13)
   {
      startStep         = std::strtol(argv[11], nullptr, 10);
      lengthOfTimeRange = std::strtol(argv[12], nullptr, 10);
   }

   FILE* f = std::fopen(inputPath, "rb");
   if (f == nullptr)
   {
      std::cerr << "Could not open " << inputPath << "\n";
      return 1;
   }

   codes_handle* h = nullptr;
   if (shortName.empty())
   {
      int err = 0;
      h       = codes_grib_handle_new_from_file(nullptr, f, &err);
      if (h == nullptr || err != 0)
      {
         std::fclose(f);
         std::cerr << "codes_grib_new_from_file failed: "
                   << codes_get_error_message(err) << "\n";
         return 1;
      }
   }
   else
   {
      try
      {
         h = FindMessage(f,
                         shortName,
                         typeOfLevel,
                         topLevel,
                         bottomLevel,
                         startStep,
                         lengthOfTimeRange);
      }
      catch (const std::exception& e)
      {
         std::fclose(f);
         std::cerr << "Error: " << e.what() << "\n";
         return 1;
      }
      if (h == nullptr)
      {
         std::fclose(f);
         std::cerr << "No message with shortName \"" << shortName << "\" in "
                   << inputPath << "\n";
         return 1;
      }
   }
   std::fclose(f);

   try
   {
      GridInfo            grid        = ReadGridInfo(h);
      std::vector<double> values      = ReadValues(h);
      const ProductInfo   productInfo = shortName.empty() ?
                                           ProductInfoFromPath(inputPath) :
                                           ProductInfoFromHandle(h, shortName);
      codes_handle_delete(h);

      if (static_cast<long>(values.size()) != grid.ni * grid.nj)
      {
         std::cerr << "Value count " << values.size()
                   << " does not match Ni*Nj " << (grid.ni * grid.nj) << "\n";
         return 1;
      }

      // Convert to the wire dtype (float32) -- WriteFrame computes its
      // own stats/missing-count from this and grid.missingValue.
      std::vector<float> wireValues(values.size());
      for (size_t i = 0; i < values.size(); ++i)
      {
         wireValues[i] = static_cast<float>(values[i]);
      }

      if (!WriteFrame(outputPath,
                      grid,
                      productInfo,
                      colorRange,
                      wireValues,
                      grid.missingValue))
      {
         return 1;
      }
   }
   catch (const std::exception& e)
   {
      std::cerr << "Error: " << e.what() << "\n";
      codes_handle_delete(h);
      return 1;
   }

   return 0;
}
