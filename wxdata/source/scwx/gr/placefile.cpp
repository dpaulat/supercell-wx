#include <scwx/gr/placefile.hpp>
#include <scwx/gr/color.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/streams.hpp>
#include <scwx/util/strings.hpp>

#include <fstream>
#include <sstream>
#include <unordered_map>

#include <boost/algorithm/string.hpp>
#include <boost/units/base_units/metric/nautical_mile.hpp>

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

   void ParseLocation(const std::string& latitudeToken,
                      const std::string& longitudeToken,
                      double&            latitude,
                      double&            longitude,
                      double&            x,
                      double&            y);
   void ProcessLine(const std::string& line);

   static void TrimQuotes(std::string& s);

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

std::vector<std::shared_ptr<Placefile::DrawItem>> Placefile::GetDrawItems()
{
   return p->drawItems_;
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

      if (line.size() >= 1)
      {
         try
         {
            switch (placefile->p->currentStatement_)
            {
            case DrawingStatement::Standard:
               placefile->p->ProcessLine(line);
               break;

            case DrawingStatement::Line:
            case DrawingStatement::Triangles:
            case DrawingStatement::Image:
            case DrawingStatement::Polygon:
               if (boost::istarts_with(line, "End:"))
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

void Placefile::Impl::ProcessLine(const std::string& line)
{
   static const std::string thresholdKey_ {"Threshold:"};
   static const std::string hsluvKey_ {"HSLuv:"};
   static const std::string colorKey_ {"Color:"};
   static const std::string refreshKey_ {"Refresh:"};
   static const std::string refreshSecondsKey_ {"RefreshSeconds:"};
   static const std::string placeKey_ {"Place:"};
   static const std::string iconFileKey_ {"IconFile:"};
   static const std::string iconKey_ {"Icon:"};
   static const std::string fontKey_ {"Font:"};
   static const std::string textKey_ {"Text:"};
   static const std::string objectKey_ {"Object:"};
   static const std::string endKey_ {"End:"};
   static const std::string lineKey_ {"Line:"};
   static const std::string trianglesKey_ {"Triangles:"};
   static const std::string imageKey_ {"Image:"};
   static const std::string polygonKey_ {"Polygon:"};

   currentStatement_ = DrawingStatement::Standard;

   // When tokenizing, add one additional delimiter to discard unexpected
   // parameters (where appropriate)

   if (boost::istarts_with(line, thresholdKey_))
   {
      // Threshold: nautical_miles
      std::vector<std::string> tokenList =
         util::ParseTokens(line, {" "}, thresholdKey_.size());

      if (tokenList.size() >= 1)
      {
         threshold_ =
            static_cast<boost::units::quantity<boost::units::si::length>>(
               std::stod(tokenList[0]) *
               boost::units::metric::nautical_mile_base_unit::unit_type());
      }
   }
   else if (boost::istarts_with(line, hsluvKey_))
   {
      // HSLuv: value
      std::vector<std::string> tokenList =
         util::ParseTokens(line, {" "}, hsluvKey_.size());

      if (tokenList.size() >= 1)
      {
         if (boost::iequals(tokenList[0], "true"))
         {
            colorMode_ = ColorMode::HSLuv;
         }
         else
         {
            colorMode_ = ColorMode::RGBA;
         }
      }
   }
   else if (boost::istarts_with(line, colorKey_))
   {
      // Color: red green blue [alpha]
      std::vector<std::string> tokenList =
         util::ParseTokens(line, {" ", " ", " ", " "}, colorKey_.size());

      if (tokenList.size() >= 3)
      {
         color_ = ParseColor(tokenList, 0, colorMode_);
      }
   }
   else if (boost::istarts_with(line, refreshKey_))
   {
      // Refresh: minutes
      std::vector<std::string> tokenList =
         util::ParseTokens(line, {" "}, refreshKey_.size());

      if (tokenList.size() >= 1)
      {
         refresh_ = std::chrono::minutes {std::stoi(tokenList[0])};
      }
   }
   else if (boost::istarts_with(line, refreshSecondsKey_))
   {
      // RefreshSeconds: seconds
      std::vector<std::string> tokenList =
         util::ParseTokens(line, {" "}, refreshSecondsKey_.size());

      if (tokenList.size() >= 1)
      {
         refresh_ = std::chrono::seconds {std::stoi(tokenList[0])};
      }
   }
   else if (boost::istarts_with(line, placeKey_))
   {
      // Place: latitude, longitude, string with spaces
      std::vector<std::string> tokenList =
         util::ParseTokens(line, {",", ","}, placeKey_.size());

      if (tokenList.size() >= 3)
      {
         std::shared_ptr<TextDrawItem> di = std::make_shared<TextDrawItem>();

         di->threshold_ = threshold_;
         di->color_     = color_;

         ParseLocation(tokenList[0],
                       tokenList[1],
                       di->latitude_,
                       di->longitude_,
                       di->x_,
                       di->y_);

         ProcessEscapeCharacters(tokenList[2]);
         di->text_.swap(tokenList[2]);

         drawItems_.emplace_back(std::move(di));
      }
      else
      {
         logger_->warn("Place statement malformed: {}", line);
      }
   }
   else if (boost::istarts_with(line, iconFileKey_))
   {
      // IconFile: fileNumber, iconWidth, iconHeight, hotX, hotY, fileName

      // TODO
   }
   else if (boost::istarts_with(line, iconKey_))
   {
      // Icon: lat, lon, angle, fileNumber, iconNumber, hoverText

      // TODO
   }
   else if (boost::istarts_with(line, fontKey_))
   {
      // Font: fontNumber, pixels, flags, "face"

      // TODO
   }
   else if (boost::istarts_with(line, textKey_))
   {
      // Text: lat, lon, fontNumber, "string", "hover"
      std::vector<std::string> tokenList =
         util::ParseTokens(line, {",", ",", ",", ",", ","}, textKey_.size());

      std::shared_ptr<TextDrawItem> di = nullptr;

      if (tokenList.size() >= 4)
      {
         di = std::make_shared<TextDrawItem>();

         di->threshold_ = threshold_;
         di->color_     = color_;

         ParseLocation(tokenList[0],
                       tokenList[1],
                       di->latitude_,
                       di->longitude_,
                       di->x_,
                       di->y_);

         di->fontNumber_ = std::stoul(tokenList[2]);

         ProcessEscapeCharacters(tokenList[3]);
         TrimQuotes(tokenList[3]);
         di->text_.swap(tokenList[3]);
      }
      if (tokenList.size() >= 5)
      {
         ProcessEscapeCharacters(tokenList[4]);
         TrimQuotes(tokenList[4]);
         di->hoverText_.swap(tokenList[4]);
      }

      if (di != nullptr)
      {
         drawItems_.emplace_back(std::move(di));
      }
      else
      {
         logger_->warn("Text statement malformed: {}", line);
      }
   }
   else if (boost::istarts_with(line, objectKey_))
   {
      // Object: lat, lon
      //    ...
      // End:
      std::vector<std::string> tokenList =
         util::ParseTokens(line, {",", ","}, objectKey_.size());

      double latitude {};
      double longitude {};

      if (tokenList.size() >= 2)
      {
         latitude  = std::stod(tokenList[0]);
         longitude = std::stod(tokenList[1]);
      }
      else
      {
         logger_->warn("Object statement malformed: {}", line);
      }

      objectStack_.emplace_back(Object {latitude, longitude});
   }
   else if (boost::istarts_with(line, endKey_))
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
   else if (boost::istarts_with(line, lineKey_))
   {
      // Line: width, flags [, hover_text]
      //    lat, lon
      //    ...
      // End:
      currentStatement_ = DrawingStatement::Line;

      // TODO
   }
   else if (boost::istarts_with(line, trianglesKey_))
   {
      // Triangles:
      //    lat, lon [, r, g, b [,a]]
      //    ...
      // End:
      currentStatement_ = DrawingStatement::Triangles;

      // TODO
   }
   else if (boost::istarts_with(line, imageKey_))
   {
      // Image: image_file
      //    lat, lon, Tu [, Tv ]
      //    ...
      // End:
      currentStatement_ = DrawingStatement::Image;

      // TODO
   }
   else if (boost::istarts_with(line, polygonKey_))
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
   else
   {
      logger_->warn("Unknown statement: {}", line);
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

void Placefile::Impl::ProcessEscapeCharacters(std::string& s)
{
   boost::replace_all(s, "\\n", "\n");
}

void Placefile::Impl::TrimQuotes(std::string& s)
{
   if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
   {
      s.erase(s.size() - 1);
      s.erase(0, 1);
   }
}

} // namespace gr
} // namespace scwx
