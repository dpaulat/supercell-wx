// Scratch validator, not part of the wire-format tool chain: implements the
// same spherical Lambert Conformal Conic math slated for
// GribProductLayer::LambertGridToLatLon and checks it against eccodes' own
// decoded "latitudes"/"longitudes" arrays for real RTMA grid points, so the
// formula is proven correct against ground truth before it goes anywhere
// near the renderer.
#include <eccodes.h>

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{

constexpr double kPi = 3.14159265358979323846;

double Radians(double deg)
{
   return deg * kPi / 180.0;
}
double Degrees(double rad)
{
   return rad * 180.0 / kPi;
}

struct LambertGrid
{
   double lov, lad, latin1, latin2, lat1, lon1, dx, dy, radius;
};

double ConeConstantN(double latin1Deg, double latin2Deg)
{
   const double phi1 = Radians(latin1Deg);
   const double phi2 = Radians(latin2Deg);
   if (std::fabs(latin1Deg - latin2Deg) < 1e-9)
   {
      return std::sin(phi1);
   }
   return std::log(std::cos(phi1) / std::cos(phi2)) /
          std::log(std::tan(kPi / 4 + phi2 / 2) / std::tan(kPi / 4 + phi1 / 2));
}

// Returns (x, y) relative to the pole (rho=0 at the pole -- see reasoning
// in the message accompanying this file). sinTheta/cosTheta convention:
// x = rho*sin(theta), y = -rho*cos(theta).
void Forward(const LambertGrid& g, double n, double F, double latDeg,
            double lonDeg, double& x, double& y)
{
   const double phi   = Radians(latDeg);
   const double theta = n * Radians(lonDeg - g.lov);
   const double rho   = g.radius * F / std::pow(std::tan(kPi / 4 + phi / 2), n);
   x = rho * std::sin(theta);
   y = -rho * std::cos(theta);
}

void Inverse(const LambertGrid& g, double n, double F, double x, double y,
            double& latDeg, double& lonDeg)
{
   const double rho   = std::copysign(std::sqrt(x * x + y * y), n);
   const double theta = std::atan2(x, -y);
   const double phi   = 2.0 * std::atan(std::pow(g.radius * F / rho, 1.0 / n)) - kPi / 2;
   latDeg             = Degrees(phi);
   lonDeg             = Degrees(theta / n) + g.lov;
}

void GridToLatLon(const LambertGrid& g, double i, double j, double& latDeg,
                  double& lonDeg)
{
   const double n = ConeConstantN(g.latin1, g.latin2);
   const double phi1 = Radians(g.latin1);
   const double F    = std::cos(phi1) * std::pow(std::tan(kPi / 4 + phi1 / 2), n) / n;

   double x0, y0;
   Forward(g, n, F, g.lat1, g.lon1, x0, y0);

   const double x = x0 + i * g.dx;
   const double y = y0 + j * g.dy;

   Inverse(g, n, F, x, y, latDeg, lonDeg);
}

} // namespace

int main()
{
   FILE* f = std::fopen("data/rtma_test.grb2", "rb");
   int   err = 0;
   codes_handle* h = codes_grib_handle_new_from_file(nullptr, f, &err);
   std::fclose(f);

   long ni = 0, nj = 0;
   codes_get_long(h, "Ni", &ni);
   codes_get_long(h, "Nj", &nj);

   size_t count = 0;
   codes_get_size(h, "values", &count);
   std::vector<double> lats(count), lons(count), values(count);
   codes_grib_get_data(h, lats.data(), lons.data(), values.data());

   LambertGrid g {};
   codes_get_double(h, "LoVInDegrees", &g.lov);
   codes_get_double(h, "LaDInDegrees", &g.lad);
   codes_get_double(h, "Latin1InDegrees", &g.latin1);
   codes_get_double(h, "Latin2InDegrees", &g.latin2);
   codes_get_double(h, "latitudeOfFirstGridPointInDegrees", &g.lat1);
   codes_get_double(h, "longitudeOfFirstGridPointInDegrees", &g.lon1);
   if (g.lon1 > 180.0) g.lon1 -= 360.0;
   if (g.lov > 180.0) g.lov -= 360.0;
   codes_get_double(h, "DxInMetres", &g.dx);
   codes_get_double(h, "DyInMetres", &g.dy);
   codes_get_double(h, "radius", &g.radius);

   std::printf("Grid: lov=%.3f lad=%.3f latin1=%.3f latin2=%.3f lat1=%.4f lon1=%.4f dx=%.2f dy=%.2f radius=%.1f\n",
               g.lov, g.lad, g.latin1, g.latin2, g.lat1, g.lon1, g.dx, g.dy, g.radius);

   // eccodes' flat "values"/"latitudes"/"longitudes" arrays are in scan
   // order: i varies fastest (jPointsAreConsecutive=0, confirmed earlier),
   // j from 0. So flat index = j*ni + i, matching what GridToLatLon(i,j)
   // should reproduce.
   auto checkPoint = [&](long i, long j)
   {
      const size_t idx = static_cast<size_t>(j) * static_cast<size_t>(ni) + static_cast<size_t>(i);
      double predLat, predLon;
      GridToLatLon(g, static_cast<double>(i), static_cast<double>(j), predLat, predLon);
      double predLonNorm = predLon;
      if (predLonNorm < 0) predLonNorm += 360.0;
      std::printf("(i=%5ld,j=%5ld) eccodes=(%.5f,%.5f) mine=(%.5f,%.5f) diff=(%.6f,%.6f)\n",
                  i, j, lats[idx], lons[idx], predLat, predLonNorm,
                  predLat - lats[idx], predLonNorm - lons[idx]);
   };

   checkPoint(0, 0);
   checkPoint(ni - 1, 0);
   checkPoint(0, nj - 1);
   checkPoint(ni - 1, nj - 1);
   checkPoint(ni / 2, nj / 2);
   checkPoint(1000, 800);
   checkPoint(200, 1400);

   codes_handle_delete(h);
   return 0;
}
