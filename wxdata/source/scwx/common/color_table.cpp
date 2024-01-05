#include <scwx/common/color_table.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/streams.hpp>

#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>

#include <boost/algorithm/string.hpp>
#include <boost/gil.hpp>

#include <hsluv.h>

namespace scwx
{
namespace common
{

static const std::string logPrefix_ {"scwx::common::color_table"};
static const auto        logger_ = util::Logger::Create(logPrefix_);

enum class ColorMode
{
   RGBA,
   HSLuv
};

static boost::gil::rgba8_pixel_t
ParseColor(const std::vector<std::string>& tokenList,
           size_t                          startIndex,
           ColorMode                       colorMode,
           bool                            hasAlpha = true);
template<typename T>
T RoundChannel(double value);
template<typename T>
T StringToDecimal(const std::string& str);

class ColorTableImpl
{
public:
   explicit ColorTableImpl() :
       product_ {},
       units_ {},
       scale_ {1.0f},
       offset_ {0.0f},
       step_ {10},
       rfColor_ {0, 0, 0, 0},
       colorMode_ {ColorMode::RGBA},
       colorMap_ {} {};
   ~ColorTableImpl() = default;

   std::string               product_;
   std::string               units_;
   float                     scale_;
   float                     offset_;
   long                      step_;
   boost::gil::rgba8_pixel_t rfColor_;
   ColorMode                 colorMode_;

   std::map<float,
            std::pair<boost::gil::rgba8_pixel_t,
                      std::optional<boost::gil::rgba8_pixel_t>>>
      colorMap_;
};

ColorTable::ColorTable() : p(std::make_unique<ColorTableImpl>()) {}
ColorTable::~ColorTable() = default;

ColorTable::ColorTable(ColorTable&&) noexcept            = default;
ColorTable& ColorTable::operator=(ColorTable&&) noexcept = default;

std::string ColorTable::units() const
{
   return p->units_;
}

float ColorTable::scale() const
{
   return p->scale_;
}

float ColorTable::offset() const
{
   return p->offset_;
}

boost::gil::rgba8_pixel_t ColorTable::rf_color() const
{
   return p->rfColor_;
}

boost::gil::rgba8_pixel_t ColorTable::Color(float value) const
{
   boost::gil::rgba8_pixel_t color;
   bool                      found = false;

   value = value * p->scale_ + p->offset_;

   auto prev = p->colorMap_.cbegin();
   for (auto it = p->colorMap_.cbegin(); it != p->colorMap_.cend(); ++it)
   {
      if (value < it->first)
      {
         if (it == p->colorMap_.cbegin())
         {
            color = it->second.first;
         }
         else
         {
            // Interpolate
            float key1 = prev->first;
            float key2 = it->first;

            boost::gil::rgba8_pixel_t color1 = prev->second.first;
            boost::gil::rgba8_pixel_t color2 = (prev->second.second) ?
                                                  prev->second.second.value() :
                                                  it->second.first;

            float t = (value - key1) / (key2 - key1);
            color[0] =
               RoundChannel<uint8_t>(std::lerp(color1[0], color2[0], t));
            color[1] =
               RoundChannel<uint8_t>(std::lerp(color1[1], color2[1], t));
            color[2] =
               RoundChannel<uint8_t>(std::lerp(color1[2], color2[2], t));
            color[3] =
               RoundChannel<uint8_t>(std::lerp(color1[3], color2[3], t));
         }

         found = true;
         break;
      }

      prev = it;
   }

   if (!found)
   {
      color = prev->second.first;
   }

   return color;
}

bool ColorTable::IsValid() const
{
   return p->colorMap_.size() > 0;
}

std::shared_ptr<ColorTable> ColorTable::Load(const std::string& filename)
{
   logger_->debug("Loading color table: {}", filename);
   std::ifstream f(filename, std::ios_base::in);
   return Load(f);
}

std::shared_ptr<ColorTable> ColorTable::Load(std::istream& is)
{
   std::shared_ptr<ColorTable> p = std::make_shared<ColorTable>();

   std::string line;
   while (scwx::util::getline(is, line))
   {
      std::string              token;
      std::istringstream       tokens(line);
      std::vector<std::string> tokenList;

      while (tokens >> token)
      {
         if (token.find(';') != std::string::npos)
         {
            break;
         }

         tokenList.push_back(std::move(token));
      }

      if (tokenList.size() >= 2)
      {
         try
         {
            p->ProcessLine(tokenList);
         }
         catch (const std::exception&)
         {
            logger_->warn("Could not parse line: {}", line);
         }
      }
   }

   return p;
}

void ColorTable::ProcessLine(const std::vector<std::string>& tokenList)
{
   if (boost::iequals(tokenList[0], "Product:"))
   {
      // Product: string
      p->product_ = tokenList[1];
   }
   else if (boost::iequals(tokenList[0], "Units:"))
   {
      // Units: string
      p->units_ = tokenList[1];
   }
   else if (boost::iequals(tokenList[0], "Scale:"))
   {
      // Scale: float
      p->scale_ = std::stof(tokenList[1]);
   }
   else if (boost::iequals(tokenList[0], "Offset:"))
   {
      // Offset: float
      p->offset_ = std::stof(tokenList[1]);
   }
   else if (boost::iequals(tokenList[0], "Step:"))
   {
      // Step: number
      p->step_ = std::stol(tokenList[1]);
   }
   else if (boost::iequals(tokenList[0], "RF:"))
   {
      // RF: R G B [A]
      p->rfColor_ = ParseColor(tokenList, 1, p->colorMode_);
   }
   else if (boost::iequals(tokenList[0], "Color:"))
   {
      // Color: value R G B [R G B]
      float key = std::stof(tokenList[1]);

      boost::gil::rgba8_pixel_t color1 =
         ParseColor(tokenList, 2, p->colorMode_, false);
      std::optional<boost::gil::rgba8_pixel_t> color2;

      if (tokenList.size() >= 8)
      {
         color2 = ParseColor(tokenList, 5, p->colorMode_, false);
      }

      p->colorMap_[key] = std::make_pair(color1, color2);
   }
   else if (boost::iequals(tokenList[0], "Color4:"))
   {
      // Color4: value R G B A [R G B A]
      float key = std::stof(tokenList[1]);

      boost::gil::rgba8_pixel_t color1 =
         ParseColor(tokenList, 2, p->colorMode_);
      std::optional<boost::gil::rgba8_pixel_t> color2;

      if (tokenList.size() >= 10)
      {
         color2 = ParseColor(tokenList, 6, p->colorMode_);
      }

      p->colorMap_[key] = std::make_pair(color1, color2);
   }
   else if (boost::iequals(tokenList[0], "SolidColor:"))
   {
      // SolidColor: value R G B
      float key = std::stof(tokenList[1]);

      boost::gil::rgba8_pixel_t color1 =
         ParseColor(tokenList, 2, p->colorMode_, false);

      p->colorMap_[key] = std::make_pair(color1, color1);
   }
   else if (boost::iequals(tokenList[0], "SolidColor4:"))
   {
      // SolidColor4: value R G B A
      float key = std::stof(tokenList[1]);

      boost::gil::rgba8_pixel_t color1 =
         ParseColor(tokenList, 2, p->colorMode_);

      p->colorMap_[key] = std::make_pair(color1, color1);
   }
}

static boost::gil::rgba8_pixel_t
ParseColor(const std::vector<std::string>& tokenList,
           size_t                          startIndex,
           ColorMode                       colorMode,
           bool                            hasAlpha)
{

   uint8_t r {};
   uint8_t g {};
   uint8_t b {};
   uint8_t a = 255;

   if (colorMode == ColorMode::RGBA)
   {
      if (tokenList.size() >= startIndex + 3)
      {
         r = StringToDecimal<uint8_t>(tokenList[startIndex + 0]);
         g = StringToDecimal<uint8_t>(tokenList[startIndex + 1]);
         b = StringToDecimal<uint8_t>(tokenList[startIndex + 2]);
      }

      if (hasAlpha && tokenList.size() >= startIndex + 4)
      {
         a = StringToDecimal<uint8_t>(tokenList[startIndex + 3]);
      }
   }
   else // if (colorMode == ColorMode::HSLuv)
   {
      double h {};
      double s {};
      double l {};

      if (tokenList.size() >= startIndex + 3)
      {
         h = std::stod(tokenList[startIndex + 0]);
         s = std::stod(tokenList[startIndex + 1]);
         l = std::stod(tokenList[startIndex + 2]);
      }

      double dr;
      double dg;
      double db;

      hsluv2rgb(h, s, l, &dr, &dg, &db);

      r = RoundChannel<uint8_t>(dr * 255.0);
      g = RoundChannel<uint8_t>(dg * 255.0);
      b = RoundChannel<uint8_t>(db * 255.0);
   }

   return boost::gil::rgba8_pixel_t {r, g, b, a};
}

template<typename T>
T RoundChannel(double value)
{
   return static_cast<T>(std::clamp<int>(std::lround(value),
                                         std::numeric_limits<T>::min(),
                                         std::numeric_limits<T>::max()));
}

template<typename T>
T StringToDecimal(const std::string& str)
{
   return static_cast<T>(std::clamp<int>(std::stoi(str),
                                         std::numeric_limits<T>::min(),
                                         std::numeric_limits<T>::max()));
}

} // namespace common
} // namespace scwx
