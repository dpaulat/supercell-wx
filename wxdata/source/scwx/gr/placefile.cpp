#include <scwx/gr/placefile.hpp>
#include <scwx/gr/color.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/streams.hpp>

#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_map>

#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <boost/units/base_units/metric/nautical_mile.hpp>
#include <boost/units/quantity.hpp>
#include <boost/units/systems/si/length.hpp>

namespace scwx
{
namespace gr
{

static const std::string logPrefix_ {"scwx::gr::placefile"};
static const auto        logger_ = util::Logger::Create(logPrefix_);

enum class DrawingStatement
{
   Standard,
   Line,
   Triangles,
   Image,
   Polygon
};

class Placefile::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;

   struct Object
   {
      double x_ {};
      double y_ {};
   };

   struct DrawItem
   {
   };

   struct PlaceDrawItem : DrawItem
   {
      boost::units::quantity<boost::units::si::length> threshold_ {};
      boost::gil::rgba8_pixel_t                        color_ {};
      double                                           latitude_ {};
      double                                           longitude_ {};
      double                                           x_ {};
      double                                           y_ {};
      std::string                                      text_ {};
   };

   void ParseLocation(const std::string& latitudeToken,
                      const std::string& longitudeToken,
                      double&            latitude,
                      double&            longitude,
                      double&            x,
                      double&            y);
   void ProcessLine(const std::string&              line,
                    const std::vector<std::string>& tokenList);

   std::chrono::seconds refresh_ {-1};

   // Parsing state
   boost::units::quantity<boost::units::si::length> threshold_ {
      999.0 * boost::units::metric::nautical_mile_base_unit::unit_type()};
   boost::gil::rgba8_pixel_t color_ {255, 255, 255, 255};
   ColorMode                 colorMode_ {ColorMode::RGBA};
   std::vector<Object>       objectStack_ {};
   DrawingStatement          currentStatement_ {DrawingStatement::Standard};

   // References
   std::unordered_map<std::size_t, bool> iconFiles_ {};
   std::unordered_map<std::size_t, bool> fonts_ {};

   std::vector<std::shared_ptr<DrawItem>> drawItems_ {};
};

Placefile::Placefile() : p(std::make_unique<Impl>()) {}
Placefile::~Placefile() = default;

Placefile::Placefile(Placefile&&) noexcept            = default;
Placefile& Placefile::operator=(Placefile&&) noexcept = default;

bool Placefile::IsValid() const
{
   return p->drawItems_.size() > 0;
}

std::shared_ptr<Placefile> Placefile::Load(const std::string& filename)
{
   logger_->debug("Loading placefile: {}", filename);
   std::ifstream f(filename, std::ios_base::in);
   return Load(f);
}

std::shared_ptr<Placefile> Placefile::Load(std::istream& is)
{
   std::shared_ptr<Placefile> placefile = std::make_shared<Placefile>();

   std::string line;
   while (scwx::util::getline(is, line))
   {
      // Find position of comment (;)
      bool inQuotes = false;
      for (std::size_t i = 0; i < line.size(); ++i)
      {
         if (!inQuotes && line[i] == ';')
         {
            // Remove comment
            line.erase(i);
            break;
         }
         else if (line[i] == '"')
         {
            // Toggle quote state
            inQuotes = !inQuotes;
         }
      }

      // Remove extra spacing from line
      boost::trim(line);

      boost::char_separator<char> delimiter(", ");
      boost::tokenizer            tokens(line, delimiter);
      std::vector<std::string>    tokenList;

      for (auto& token : tokens)
      {
         tokenList.push_back(token);
      }

      if (tokenList.size() >= 1)
      {
         try
         {
            switch (placefile->p->currentStatement_)
            {
            case DrawingStatement::Standard:
               placefile->p->ProcessLine(line, tokenList);
               break;

            case DrawingStatement::Line:
            case DrawingStatement::Triangles:
            case DrawingStatement::Image:
            case DrawingStatement::Polygon:
               if (boost::iequals(tokenList[0], "End:"))
               {
                  placefile->p->currentStatement_ = DrawingStatement::Standard;
               }
               break;
            }
         }
         catch (const std::exception&)
         {
            logger_->warn("Could not parse line: {}", line);
         }
      }
   }

   return placefile;
}

void Placefile::Impl::ProcessLine(const std::string&              line,
                                  const std::vector<std::string>& tokenList)
{
   currentStatement_ = DrawingStatement::Standard;

   if (boost::iequals(tokenList[0], "Threshold:"))
   {
      // Threshold: nautical_miles
      if (tokenList.size() >= 2)
      {
         threshold_ =
            static_cast<boost::units::quantity<boost::units::si::length>>(
               std::stod(tokenList[1]) *
               boost::units::metric::nautical_mile_base_unit::unit_type());
      }
   }
   else if (boost::iequals(tokenList[0], "HSLuv:"))
   {
      // HSLuv: value
      if (tokenList.size() >= 2)
      {
         if (boost::iequals(tokenList[1], "true"))
         {
            colorMode_ = ColorMode::HSLuv;
         }
         else
         {
            colorMode_ = ColorMode::RGBA;
         }
      }
   }
   else if (boost::iequals(tokenList[0], "Color:"))
   {
      // Color: red green blue
      if (tokenList.size() >= 2)
      {
         color_ = ParseColor(tokenList, 1, colorMode_);
      }
   }
   else if (boost::iequals(tokenList[0], "Refresh:"))
   {
      // Refresh: minutes
      if (tokenList.size() >= 2)
      {
         refresh_ = std::chrono::minutes {std::stoi(tokenList[1])};
      }
   }
   else if (boost::iequals(tokenList[0], "RefreshSeconds:"))
   {
      // RefreshSeconds: seconds
      if (tokenList.size() >= 2)
      {
         refresh_ = std::chrono::seconds {std::stoi(tokenList[1])};
      }
   }
   else if (boost::iequals(tokenList[0], "Place:"))
   {
      // Place: latitude, longitude, string with spaces
      std::regex  re {"Place:\\s*([+\\-0-9\\.]+),\\s*([+\\-0-9\\.]+),\\s*(.+)"};
      std::smatch match;
      std::regex_match(line, match, re);

      if (match.size() >= 4)
      {
         std::shared_ptr<PlaceDrawItem> di = std::make_shared<PlaceDrawItem>();

         di->threshold_ = threshold_;
         di->color_     = color_;

         ParseLocation(match[1].str(),
                       match[2].str(),
                       di->latitude_,
                       di->longitude_,
                       di->x_,
                       di->y_);

         di->text_ = match[3].str();

         drawItems_.emplace_back(std::move(di));
      }
      else
      {
         logger_->warn("Place statement malformed: {}", line);
      }
   }
   else if (boost::iequals(tokenList[0], "IconFile:"))
   {
      // IconFile: fileNumber, iconWidth, iconHeight, hotX, hotY, fileName

      // TODO
   }
   else if (boost::iequals(tokenList[0], "Icon:"))
   {
      // Icon: lat, lon, angle, fileNumber, iconNumber, hoverText

      // TODO
   }
   else if (boost::iequals(tokenList[0], "Font:"))
   {
      // Font: fontNumber, pixels, flags, "face"

      // TODO
   }
   else if (boost::iequals(tokenList[0], "Text:"))
   {
      // Text: lat, lon, fontNumber, "string", "hover"

      // TODO
   }
   else if (boost::iequals(tokenList[0], "Object:"))
   {
      // Object: lat, lon
      //    ...
      // End:
      std::regex  re {"Object:\\s*([+\\-0-9\\.]+),\\s*([+\\-0-9\\.]+)"};
      std::smatch match;
      std::regex_match(line, match, re);

      double latitude {};
      double longitude {};

      if (match.size() >= 3)
      {
         latitude  = std::stod(match[1].str());
         longitude = std::stod(match[2].str());
      }
      else
      {
         logger_->warn("Object statement malformed: {}", line);
      }

      objectStack_.emplace_back(Object {latitude, longitude});
   }
   else if (boost::iequals(tokenList[0], "End:"))
   {
      // Object End
      if (!objectStack_.empty())
      {
         objectStack_.pop_back();
      }
      else
      {
         logger_->warn("End found without Object");
      }
   }
   else if (boost::iequals(tokenList[0], "Line:"))
   {
      // Line: width, flags [, hover_text]
      //    lat, lon
      //    ...
      // End:
      currentStatement_ = DrawingStatement::Line;

      // TODO
   }
   else if (boost::iequals(tokenList[0], "Triangles:"))
   {
      // Triangles:
      //    lat, lon [, r, g, b [,a]]
      //    ...
      // End:
      currentStatement_ = DrawingStatement::Triangles;

      // TODO
   }
   else if (boost::iequals(tokenList[0], "Image:"))
   {
      // Image: image_file
      //    lat, lon, Tu [, Tv ]
      //    ...
      // End:
      currentStatement_ = DrawingStatement::Image;

      // TODO
   }
   else if (boost::iequals(tokenList[0], "Polygon:"))
   {
      // Polygon:
      //    lat1, lon1 [, r, g, b [,a]] ; start of the first contour
      //    ...
      //    lat1, lon1                  ; repeating the first point closes the
      //                                ; contour
      //
      //    lat2, lon2                  ; next point starts a new contour
      //    ...
      //    lat2, lon2                  ; and repeating it ends the contour
      // End:
      currentStatement_ = DrawingStatement::Polygon;

      // TODO
   }
}

void Placefile::Impl::ParseLocation(const std::string& latitudeToken,
                                    const std::string& longitudeToken,
                                    double&            latitude,
                                    double&            longitude,
                                    double&            x,
                                    double&            y)
{
   if (objectStack_.empty())
   {
      // If an Object statement is not currently open, parse latitude and
      // longitude tokens as-is
      latitude  = std::stod(latitudeToken);
      longitude = std::stod(longitudeToken);
   }
   else
   {
      // If an Object statement is open, the latitude and longitude are from the
      // outermost Object
      latitude  = objectStack_[0].x_;
      longitude = objectStack_[0].y_;

      // The latitude and longitude tokens are interpreted as x, y offsets
      x = std::stod(latitudeToken);
      y = std::stod(longitudeToken);

      // If there are inner Object statements open, treat these as x, y offsets
      for (std::size_t i = 1; i < objectStack_.size(); i++)
      {
         x += objectStack_[i].x_;
         y += objectStack_[i].y_;
      }
   }
}

} // namespace gr
} // namespace scwx
