#pragma once

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#include <glm/glm.hpp>

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

#include <cstddef>
#include <filesystem>
#include <string>

namespace scwx::qt::map
{

// Where decode_grib's output frame files and manager::GribManager's/
// manager::WindBarbManager's own S3 downloads live -- a "grib" subdirectory
// of ApplicationPaths' Cache location (see
// scwx/qt/main/application_paths.hpp), the same mechanism every other
// downloaded/generated data in the app already resolves through, so this
// respects portable mode instead of hardcoding a path. Created on first
// call if missing.
std::filesystem::path GetGribDataDirectory();

// Independent, simultaneously-visible GRIB layers, one per distinct data
// family (not one lumped "Models" -- this replaced an earlier Mrms/Models
// 2-way split: with RTMA and RRFS both live, a single shared "Models"
// product selector could only ever show one of them at a time, and the
// two are different enough -- RTMA a rolling analysis, RRFS a real
// forecast model with its own cycle/hour structure -- that forcing them
// through one dropdown was the wrong shape, not just a UI inconvenience).
// Each category gets its
// own GribProductLayer, GribManager instance (see manager::GribManager::
// Instance(GribCategory)), and GribDockWidget, so a field from each can
// be shown at once -- e.g. RTMA temperature underneath MRMS reflectivity
// underneath RRFS MSLP. Future families (REFS, NBM, ...) get their own
// enumerator the same way, not folded into an existing one.
enum class GribCategory
{
   Mrms,
   Rtma,
   Rrfs,
   Nbm
};

// Shared between GribProductLayer, manager::GribManager, and
// GribDockWidget so all three reference the same frame file for a given
// (category, product) pair -- each pair gets its own fixed path so
// nothing clobbers anything else. `productIndex` is a category's own
// product table index (see manager::GribManager::ProductNames()) --
// required, not defaulted, so GribManager can support several products
// active at once within one category, each with its own frame file --
// per product within a manager instance, not yet per pane.
std::string GetGribFramePath(GribCategory category, std::size_t productIndex);

// Reads just the "validTime" field from a binary-framed grid file's JSON
// header line (see grib-helper/README.md for the wire format). Returns an
// empty string if the file can't be read or the field is missing. (A
// "product" label reader used to live here too -- removed since every
// caller now prefers GribManager::CurrentProductName(), the curated
// display name, over decode_grib's raw per-source label; see
// grib_dock_widget.cpp/grib_product_layer.cpp.)
std::string ReadGribFrameValidTime(const std::string& framePath);

// The product's own physical value range the map's color ramp is
// stretched across for this frame -- see GribProductLayer::BuildPalette()'s
// own doc for exactly how a raw decoded value maps to a color from these
// two numbers (color = paletteColorTable.Color(-20 + t * 95) where
// t = (value - colorOffset) / colorScale, matching the shader's own
// normalization). Both fields are 0 if the file can't be read or the
// fields are missing (colorScale == 0 also means "not a real range,"
// since no real product configures a zero span).
struct GribFrameColorRange
{
   float colorOffset {0.0f};
   float colorScale {0.0f};
};
GribFrameColorRange ReadGribFrameColorRange(const std::string& framePath);

// Reads a numeric field ("key":123.4) from a frame file's JSON-ish header
// line. Throws std::runtime_error if the key is missing.
double ExtractNumber(const std::string& header, const std::string& key);

// Same as ExtractNumber, but returns `defaultValue` instead of throwing if
// the key is missing -- for fields added after the wire format's first
// version, so an older decode_grib's frame still loads.
double ExtractNumberOr(const std::string& header,
                       const std::string& key,
                       double             defaultValue);

// Reads a string field ("key":"value") from a frame file's JSON-ish header
// line. Throws std::runtime_error if the key is missing.
std::string ExtractString(const std::string& header, const std::string& key);

// Same as ExtractString, but returns `defaultValue` instead of throwing if
// the key is missing.
std::string ExtractStringOr(const std::string& header,
                            const std::string& key,
                            const std::string& defaultValue);

// Lambert Conformal Conic projection parameters, fully describing the cone
// (lov/lad/latin1/latin2/radius) and a grid's placement on it (lat1/lon1 at
// grid index (0, 0), dx/dy grid spacing in the projected plane). Reference:
// Snyder, "Map Projections: A Working Manual" (USGS PP 1395), section 15 --
// forward eqs 15-1 to 15-3, inverse eqs 15-8 to 15-10 (spherical case;
// every NCEP CONUS-nest grid checked so far declares shapeOfTheEarth=1,
// i.e. spherical with an explicit radius -- see decode_grib's "radius"
// field -- so the ellipsoidal variant is not needed here). Shared by every
// consumer of a lambert-grid frame file (GribProductLayer for Models/RTMA
// today, WindBarbLayer for RTMA's wind fields).
struct LambertGrid
{
   double lov;
   double lad;
   double latin1;
   double latin2;
   double lat1;
   double lon1;
   double dx;
   double dy;
   double radius;
};

// Cone constant n and the F factor Snyder's rho formula shares between
// every point projected on this cone -- computed once per grid (not once
// per point) since forward and inverse both need the identical values.
// Uses latin1 as the reference parallel for F; any parallel works, latin1
// is just Snyder's own convention. The latin1==latin2 (tangent cone) case
// is handled explicitly: the general secant formula for n is a 0/0 form
// there (both numerator and denominator vanish when latin1==latin2, which
// is RTMA's actual case -- 25/25 -- so this isn't just a defensive edge
// case, it is the common case), and its L'Hopital limit is sin(phi1).
struct LambertConstants
{
   double n;
   double f;
};

LambertConstants ComputeLambertConstants(const LambertGrid& grid);

// Forward Lambert projection (Snyder eqs 15-1 to 15-3), returning (x, y)
// relative to the cone's apex (the pole's image under this projection) --
// not relative to any GRIB-defined false origin. Callers only ever need
// *differences* in (x, y), so an arbitrary but consistent origin for both
// the forward and inverse steps is all that's required, and the pole is
// the natural, false-origin-free choice for it.
glm::dvec2 LambertForward(const LambertGrid&      grid,
                          const LambertConstants& c,
                          double                  latDeg,
                          double                  lonDeg);

// Inverse Lambert projection (Snyder eqs 15-8 to 15-10), the algebraic
// inverse of LambertForward using the same (n, F) constants and the same
// pole-relative (x, y) convention.
glm::dvec2 LambertInverse(const LambertGrid&      grid,
                          const LambertConstants& c,
                          double                  x,
                          double                  y);

// Converts fractional grid indices (i, j) to true geographic (lat, lon)
// degrees. i increases eastward from grid index 0; j increases in
// whichever direction this grid actually scans from (lat1, lon1) -- for
// RTMA specifically, confirmed via grib_ls that iScansNegatively=0 and
// jScansPositively=1, i.e. (lat1, lon1) is the *south*-west corner and j
// increases northward, so grid.dy is applied as +j*dy from that SW origin.
// Validated against eccodes' own decoded "latitudes"/"longitudes" arrays
// for the real RTMA grid (all four corners plus several interior points)
// before this went anywhere near the renderer -- see
// grib-helper/src/validate_lcc.cpp.
glm::dvec2 LambertGridToLatLon(const LambertGrid& grid, double i, double j);

} // namespace scwx::qt::map
