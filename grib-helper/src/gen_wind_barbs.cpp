// One-off asset-generation tool, not part of decode_grib's build (compile
// directly: g++ -std=c++20 gen_wind_barbs.cpp -o gen_wind_barbs). Emits one
// SVG file per 5-knot wind speed bucket (0..100kt) in each of two colors,
// standard meteorological wind barb convention: shaft points toward where
// the wind is blowing FROM (the reference icon here represents a north
// wind, shaft pointing up -- GeoIcons::SetIconAngle rotates the whole icon
// per-instance to match real data, so only this one reference orientation
// is needed); pennants (triangular flags) = 50kt, full barbs = 10kt, half
// barbs = 5kt, placed on the shaft nearest its far (upwind) end. Calm
// (0kt) is a plain circle at the station point, no shaft, per convention.
//
// The red set (barb_gust_NNN.svg) exists to be drawn *underneath* the
// normal set at the same point/rotation but sized to the gust speed
// bucket instead of sustained -- the real technique wX itself uses (see
// NexradLevel3WindBarbs.cpp/NexradWidget.cpp in the wxqt repo: gust drawn
// first in red, sustained drawn second on top), not a specially-designed
// two-tone icon. Since both sets place their decorations at identical
// positions counting outward from the same shaft-top reference point, and
// gust speed is always >= sustained speed, the sustained icon's opaque
// decorations exactly cover the overlapping portion of the red gust
// icon's decorations underneath -- only the gust icon's *excess* length
// (gust minus sustained, in whole barb/pennant units) ends up visible,
// with no excess math needed anywhere in the renderer.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{

constexpr double kPi = 3.14159265358979323846;

// Fixed canvas for every icon, so the station point stays put visually as
// the data (and thus which bucket is shown) changes.
constexpr int    kCanvas    = 64;
constexpr double kAnchorX   = 32.0;
constexpr double kAnchorY   = 56.0; // station point (bottom)
constexpr double kShaftTopY = 8.0;  // far (upwind) end of the shaft

constexpr double kBarbSpacing  = 8.0;          // along the shaft, between barbs
constexpr double kFullBarbLen  = 16.0;         // outward length of a full barb
constexpr double kHalfBarbLen  = 8.0;          // outward length of a half barb
constexpr double kBarbAngleDeg = 60.0;         // from the shaft
constexpr double kPennantWidth = kBarbSpacing; // base length along shaft

// Barbs angle out to the LEFT of the shaft (looking from the station
// toward the far end), i.e. -x here. Real meteorological convention (
// verified against two independent canonical station-model examples: a
// north wind's barbs sit on the shaft's east side, a northwest wind's on
// its northeast side -- both are "90 degrees clockwise from the tip
// direction," the actual fixed rule) puts them on the *right* -- but
// WindBarbLayer's own GeoIcons-based rotation pipeline was found to net
// mirror an icon's internal content once wired up for real RTMA data
// (confirmed via a user-reported screenshot: barbs pointed the correct
// compass direction after negating the input angle -- see
// WindBarbLayer::Impl::RebuildIcons's own comment -- but a plain angle
// negation only changes which way a *proper* rotation spins; it cannot
// undo a *reflection* baked in elsewhere in that pipeline, and a
// reflection is exactly what would leave the tip pointing correctly while
// mirroring which side the barbs sit on). Mirroring the convention here,
// at the one place it's cheap and safe to correct, cancels that
// downstream reflection without touching the already-verified tip-
// pointing rotation logic.
struct Point
{
   double x, y;
};

Point OutwardPoint(double shaftY, double length)
{
   // Shaft runs along +y (down) to -y (up) in this reference orientation;
   // "outward" from the shaft at kBarbAngleDeg from vertical, to the left
   // (-x) -- see this function's own doc above for why.
   const double angleFromVertical = kBarbAngleDeg * kPi / 180.0;
   return {kAnchorX - length * std::sin(angleFromVertical),
           shaftY + length * std::cos(angleFromVertical)};
}

std::string BuildSvg(int speedKnots, const std::string& color)
{
   std::string body;

   if (speedKnots <= 0)
   {
      // Calm: open circle at the station point, no shaft.
      body += "<circle cx=\"" + std::to_string(kAnchorX) + "\" cy=\"" +
              std::to_string(kAnchorY) + "\" r=\"5\" fill=\"none\" stroke=\"" +
              color + "\" stroke-width=\"2\"/>\n";
   }
   else
   {
      // Shaft.
      body += "<line x1=\"" + std::to_string(kAnchorX) + "\" y1=\"" +
              std::to_string(kAnchorY) + "\" x2=\"" + std::to_string(kAnchorX) +
              "\" y2=\"" + std::to_string(kShaftTopY) + "\" stroke=\"" + color +
              "\" stroke-width=\"2\" stroke-linecap=\"round\"/>\n";

      // Decompose into pennants (50kt), full barbs (10kt), half barbs
      // (5kt) -- speedKnots is always a multiple of 5 by construction
      // (the bucket itself), so no remainder ever falls through.
      int remaining = speedKnots;
      int pennants  = remaining / 50;
      remaining %= 50;
      int fullBarbs = remaining / 10;
      remaining %= 10;
      int halfBarbs = remaining / 5;

      // Work inward from the shaft's far (upwind) end, same starting
      // point every icon so the busiest ones don't visually collide with
      // the station point.
      double y = kShaftTopY;

      for (int i = 0; i < pennants; ++i)
      {
         const Point tip  = OutwardPoint(y, kFullBarbLen);
         const Point base = {kAnchorX, y + kPennantWidth};
         body += "<polygon points=\"" + std::to_string(kAnchorX) + "," +
                 std::to_string(y) + " " + std::to_string(tip.x) + "," +
                 std::to_string(tip.y) + " " + std::to_string(base.x) + "," +
                 std::to_string(base.y) + "\" fill=\"" + color +
                 "\" stroke=\"" + color +
                 "\" stroke-width=\"1\" stroke-linejoin=\"round\"/>\n";
         y += kPennantWidth;
      }

      for (int i = 0; i < fullBarbs; ++i)
      {
         const Point tip = OutwardPoint(y, kFullBarbLen);
         body += "<line x1=\"" + std::to_string(kAnchorX) + "\" y1=\"" +
                 std::to_string(y) + "\" x2=\"" + std::to_string(tip.x) +
                 "\" y2=\"" + std::to_string(tip.y) + "\" stroke=\"" + color +
                 "\" stroke-width=\"2\" stroke-linecap=\"round\"/>\n";
         y += kBarbSpacing;
      }

      for (int i = 0; i < halfBarbs; ++i)
      {
         const Point tip = OutwardPoint(y, kHalfBarbLen);
         body += "<line x1=\"" + std::to_string(kAnchorX) + "\" y1=\"" +
                 std::to_string(y) + "\" x2=\"" + std::to_string(tip.x) +
                 "\" y2=\"" + std::to_string(tip.y) + "\" stroke=\"" + color +
                 "\" stroke-width=\"2\" stroke-linecap=\"round\"/>\n";
         y += kBarbSpacing;
      }
   }

   return "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" +
          std::to_string(kCanvas) + "\" height=\"" + std::to_string(kCanvas) +
          "\" viewBox=\"0 0 " + std::to_string(kCanvas) + " " +
          std::to_string(kCanvas) + "\">\n" + body + "</svg>\n";
}

} // namespace

int main()
{
   // Relative to the current working directory -- run from the repo root
   // to regenerate scwx-qt/res/icons/wind-barbs/ in place.
   const std::filesystem::path outDir = "scwx-qt/res/icons/wind-barbs";
   std::filesystem::create_directories(outDir);

   for (int speed = 0; speed <= 100; speed += 5)
   {
      char filename[64];
      std::snprintf(filename, sizeof(filename), "barb_%03d.svg", speed);

      std::ofstream out(outDir / filename);
      out << BuildSvg(speed, "black");

      std::printf("Wrote %s\n", (outDir / filename).c_str());
   }

   for (int speed = 0; speed <= 100; speed += 5)
   {
      char filename[64];
      std::snprintf(filename, sizeof(filename), "barb_gust_%03d.svg", speed);

      std::ofstream out(outDir / filename);
      out << BuildSvg(speed, "red");

      std::printf("Wrote %s\n", (outDir / filename).c_str());
   }

   return 0;
}
