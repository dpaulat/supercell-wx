// Companion to decode_grib.cpp: reads a binary-framed grid file back
// (JSON header line + raw float32 payload) without any JSON library or
// knowledge of GRIB/eccodes, exactly as scwx::qt::manager::GribManager
// would on the real Supercell Wx side. Proves the framing round-trips:
// stats computed here should match decode_grib's stdout output.

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace
{

// Minimal "find this numeric field in a flat JSON object" helper -- a real
// reader uses whatever JSON library Supercell already links, this is only
// here to independently verify the header without reusing decode_grib's
// own writer code.
double ExtractNumber(const std::string& header, const std::string& key)
{
   const std::string needle = "\"" + key + "\":";
   const size_t       pos    = header.find(needle);
   if (pos == std::string::npos)
   {
      throw std::runtime_error("Missing key: " + key);
   }
   return std::stod(header.substr(pos + needle.size()));
}

std::string ExtractString(const std::string& header, const std::string& key)
{
   const std::string needle = "\"" + key + "\":\"";
   const size_t       start  = header.find(needle);
   if (start == std::string::npos)
   {
      throw std::runtime_error("Missing key: " + key);
   }
   const size_t valueStart = start + needle.size();
   const size_t valueEnd   = header.find('"', valueStart);
   if (valueEnd == std::string::npos)
   {
      throw std::runtime_error("Unterminated string for key: " + key);
   }
   return header.substr(valueStart, valueEnd - valueStart);
}

} // namespace

int main(int argc, char** argv)
{
   if (argc != 2)
   {
      std::cerr << "Usage: " << argv[0] << " <frame file>\n";
      return 1;
   }

   std::ifstream in(argv[1], std::ios::binary);
   if (!in)
   {
      std::cerr << "Could not open " << argv[1] << "\n";
      return 1;
   }

   std::string header;
   std::getline(in, header);

   const std::string product   = ExtractString(header, "product");
   const std::string validTime = ExtractString(header, "validTime");
   const auto   nx           = static_cast<long>(ExtractNumber(header, "nx"));
   const auto   ny           = static_cast<long>(ExtractNumber(header, "ny"));
   const double missingValue = ExtractNumber(header, "missingValue");
   const auto   byteLength =
      static_cast<size_t>(ExtractNumber(header, "byteLength"));

   std::vector<float> values(byteLength / sizeof(float));
   in.read(reinterpret_cast<char*>(values.data()),
            static_cast<std::streamsize>(byteLength));

   if (!in)
   {
      std::cerr << "Short read: expected " << byteLength
                 << " payload bytes\n";
      return 1;
   }

   if (values.size() != static_cast<size_t>(nx * ny))
   {
      std::cerr << "Payload size " << values.size()
                 << " doesn't match nx*ny " << (nx * ny) << "\n";
      return 1;
   }

   double minVal       = std::numeric_limits<double>::infinity();
   double maxVal        = -std::numeric_limits<double>::infinity();
   double sum           = 0.0;
   size_t missingCount = 0;

   for (float v : values)
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

   const size_t validCount = values.size() - missingCount;
   const double meanVal    = validCount > 0 ? sum / validCount : 0.0;

   std::cout << "Read frame " << argv[1] << "\n"
              << "  Product:  " << product << "\n"
              << "  Valid:    " << validTime << "\n"
              << "  Header:   " << header << "\n"
              << "  Grid:     " << nx << " x " << ny << " (" << values.size()
              << " cells)\n"
              << "  Missing:  " << missingCount << " / " << values.size()
              << " cells\n"
              << "  Range:    [" << minVal << ", " << maxVal
              << "], mean=" << meanVal << "\n";

   return 0;
}
