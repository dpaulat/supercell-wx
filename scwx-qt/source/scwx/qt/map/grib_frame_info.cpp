#include <scwx/qt/map/grib_frame_info.hpp>
#include <scwx/qt/main/application_paths.hpp>

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#include <glm/gtc/constants.hpp>

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

#include <cmath>
#include <fstream>
#include <stdexcept>
#include <system_error>

#include <fmt/format.h>

namespace scwx::qt::map
{

namespace
{

std::string ReadHeaderLine(const std::string& framePath)
{
   std::ifstream in(framePath, std::ios::binary);
   if (!in)
   {
      return {};
   }

   std::string header;
   std::getline(in, header);
   return header;
}

} // namespace

std::filesystem::path GetGribDataDirectory()
{
   static const std::filesystem::path dir = []
   {
      auto            path = main::ApplicationPaths::GetLocation(
                                main::ApplicationPaths::StandardLocation::Cache) /
                             "grib";
      std::error_code ec;
      std::filesystem::create_directories(path, ec);
      return path;
   }();
   return dir;
}

std::string GetGribFramePath(GribCategory category, std::size_t productIndex)
{
   std::string prefix;
   switch (category)
   {
   case GribCategory::Mrms:
      prefix = "mrms";
      break;
   case GribCategory::Rtma:
      prefix = "rtma";
      break;
   case GribCategory::Rrfs:
      prefix = "rrfs";
      break;
   case GribCategory::Nbm:
      prefix = "nbm";
      break;
   }
   if (prefix.empty())
   {
      return {};
   }

   return (GetGribDataDirectory() /
           fmt::format("{}_product{}.frame", prefix, productIndex))
      .string();
}

std::string ReadGribFrameValidTime(const std::string& framePath)
{
   const std::string header = ReadHeaderLine(framePath);
   return ExtractStringOr(header, "validTime", {});
}

GribFrameColorRange ReadGribFrameColorRange(const std::string& framePath)
{
   const std::string header = ReadHeaderLine(framePath);

   GribFrameColorRange range;
   range.colorOffset =
      static_cast<float>(ExtractNumberOr(header, "colorOffset", 0.0));
   range.colorScale =
      static_cast<float>(ExtractNumberOr(header, "colorScale", 0.0));
   return range;
}

double ExtractNumber(const std::string& header, const std::string& key)
{
   const std::string needle = "\"" + key + "\":";
   const size_t      pos    = header.find(needle);
   if (pos == std::string::npos)
   {
      throw std::runtime_error("Missing key in frame header: " + key);
   }
   return std::stod(header.substr(pos + needle.size()));
}

double ExtractNumberOr(const std::string& header,
                       const std::string& key,
                       double             defaultValue)
{
   try
   {
      return ExtractNumber(header, key);
   }
   catch (const std::runtime_error&)
   {
      return defaultValue;
   }
}

std::string ExtractString(const std::string& header, const std::string& key)
{
   const std::string needle = "\"" + key + "\":\"";
   const size_t      start  = header.find(needle);
   if (start == std::string::npos)
   {
      throw std::runtime_error("Missing key in frame header: " + key);
   }
   const size_t valueStart = start + needle.size();
   const size_t valueEnd   = header.find('"', valueStart);
   if (valueEnd == std::string::npos)
   {
      throw std::runtime_error("Unterminated string for key: " + key);
   }
   return header.substr(valueStart, valueEnd - valueStart);
}

std::string ExtractStringOr(const std::string& header,
                            const std::string& key,
                            const std::string& defaultValue)
{
   try
   {
      return ExtractString(header, key);
   }
   catch (const std::runtime_error&)
   {
      return defaultValue;
   }
}

LambertConstants ComputeLambertConstants(const LambertGrid& grid)
{
   const double phi1 = glm::radians(grid.latin1);
   const double phi2 = glm::radians(grid.latin2);

   double n;
   if (std::abs(grid.latin1 - grid.latin2) < 1e-9)
   {
      n = std::sin(phi1);
   }
   else
   {
      n = std::log(std::cos(phi1) / std::cos(phi2)) /
          std::log(std::tan(glm::quarter_pi<double>() + phi2 / 2.0) /
                   std::tan(glm::quarter_pi<double>() + phi1 / 2.0));
   }

   const double f =
      std::cos(phi1) *
      std::pow(std::tan(glm::quarter_pi<double>() + phi1 / 2.0), n) / n;

   return {n, f};
}

glm::dvec2 LambertForward(const LambertGrid&      grid,
                          const LambertConstants& c,
                          double                  latDeg,
                          double                  lonDeg)
{
   const double phi   = glm::radians(latDeg);
   const double theta = c.n * glm::radians(lonDeg - grid.lov);
   const double rho =
      grid.radius * c.f /
      std::pow(std::tan(glm::quarter_pi<double>() + phi / 2.0), c.n);

   return {rho * std::sin(theta), -rho * std::cos(theta)};
}

glm::dvec2 LambertInverse(const LambertGrid&      grid,
                          const LambertConstants& c,
                          double                  x,
                          double                  y)
{
   const double rho   = std::copysign(std::sqrt(x * x + y * y), c.n);
   const double theta = std::atan2(x, -y);
   const double phi =
      2.0 * std::atan(std::pow(grid.radius * c.f / rho, 1.0 / c.n)) -
      glm::half_pi<double>();

   return {glm::degrees(phi), glm::degrees(theta / c.n) + grid.lov};
}

glm::dvec2 LambertGridToLatLon(const LambertGrid& grid, double i, double j)
{
   const LambertConstants c = ComputeLambertConstants(grid);

   const glm::dvec2 origin = LambertForward(grid, c, grid.lat1, grid.lon1);
   const double     x      = origin.x + i * grid.dx;
   const double     y      = origin.y + j * grid.dy;

   return LambertInverse(grid, c, x, y);
}

} // namespace scwx::qt::map
