// Prevent redefinition of __cpp_lib_format
#if defined(_MSC_VER)
#   include <yvals_core.h>
#endif

// Enable chrono formatters
#ifndef __cpp_lib_format
#   define __cpp_lib_format 202110L
#endif

#include <scwx/gr/placefile.hpp>
#include <scwx/gr/color.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/streams.hpp>
#include <scwx/util/strings.hpp>

#include <fstream>
#include <sstream>
#include <unordered_map>

#include <boost/algorithm/string.hpp>

#if !defined(_MSC_VER)
#   include <date/date.h>
#endif

using namespace units::literals;

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
   void ProcessElement(const std::string& line);
   void ProcessElementEnd();
   void ProcessLine(const std::string& line);

   static void ProcessEscapeCharacters(std::string& s);
   static void TrimQuotes(std::string& s);

   std::string          name_ {};
   std::string          title_ {};
   std::chrono::seconds refresh_ {-1};

   // Parsing state
   units::length::nautical_miles<double> threshold_ {999.0_nmi};
   boost::gil::rgba8_pixel_t             color_ {255, 255, 255, 255};
   boost::gil::rgba8_pixel_t             iconModulate_ {255, 255, 255, 255};
   ColorMode                             colorMode_ {ColorMode::RGBA};
   std::chrono::sys_time<std::chrono::seconds> startTime_ {};
   std::chrono::sys_time<std::chrono::seconds> endTime_ {};

   std::vector<Object>       objectStack_ {};
   DrawingStatement          currentStatement_ {DrawingStatement::Standard};
   std::shared_ptr<DrawItem> currentDrawItem_ {nullptr};
   std::vector<PolygonDrawItem::Element> currentPolygonContour_ {};

   // References
   std::unordered_map<std::size_t, std::shared_ptr<IconFile>> iconFiles_ {};
   std::unordered_map<std::size_t, std::shared_ptr<Font>>     fonts_ {};

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

std::vector<std::shared_ptr<const Placefile::IconFile>> Placefile::icon_files()
{
   std::vector<std::shared_ptr<const Placefile::IconFile>> iconFiles {};
   iconFiles.reserve(p->iconFiles_.size());

   std::transform(p->iconFiles_.begin(),
                  p->iconFiles_.end(),
                  std::back_inserter(iconFiles),
                  [](auto& iconFile) { return iconFile.second; });

   return iconFiles;
}

std::string Placefile::name() const
{
   return p->name_;
}

std::string Placefile::title() const
{
   return p->title_;
}

std::chrono::seconds Placefile::refresh() const
{
   return p->refresh_;
}

std::unordered_map<std::size_t, std::shared_ptr<Placefile::Font>>
Placefile::fonts()
{
   return p->fonts_;
}

std::shared_ptr<Placefile::Font> Placefile::font(std::size_t i)
{
   auto it = p->fonts_.find(i);
   if (it != p->fonts_.cend())
   {
      return it->second;
   }
   return nullptr;
}

std::shared_ptr<Placefile> Placefile::Load(const std::string& filename)
{
   logger_->debug("Loading placefile: {}", filename);
   std::ifstream f(filename, std::ios_base::in);
   return Load(filename, f);
}

std::shared_ptr<Placefile> Placefile::Load(const std::string& name,
                                           std::istream&      is)
{
   std::shared_ptr<Placefile> placefile = std::make_shared<Placefile>();

   placefile->p->name_ = name;

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
                  placefile->p->ProcessElementEnd();

                  placefile->p->currentStatement_ = DrawingStatement::Standard;
                  placefile->p->currentDrawItem_  = nullptr;
               }
               else if (placefile->p->currentDrawItem_ != nullptr)
               {
                  placefile->p->ProcessElement(line);
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
   static const std::string titleKey_ {"Title:"};
   static const std::string thresholdKey_ {"Threshold:"};
   static const std::string timeRangeKey_ {"TimeRange:"};
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

   static const std::string scwxModulateIconKey_ {"scwx-ModulateIcon:"};

   currentStatement_ = DrawingStatement::Standard;

   // When tokenizing, add one additional delimiter to discard unexpected
   // parameters (where appropriate)

   if (boost::istarts_with(line, titleKey_))
   {
      // Title: title
      title_ = line.substr(titleKey_.size());
      boost::trim(title_);
   }
   else if (boost::istarts_with(line, thresholdKey_))
   {
      // Threshold: nautical_miles
      std::vector<std::string> tokenList =
         util::ParseTokens(line, {" "}, thresholdKey_.size());

      if (tokenList.size() >= 1)
      {
         threshold_ =
            units::length::nautical_miles<double>(std::stod(tokenList[0]));
      }
   }
   else if (boost::istarts_with(line, timeRangeKey_))
   {
      // TimeRange: start_time end_time
      //   (YYYY-MM-DDThh:mm:ss)
      std::vector<std::string> tokenList =
         util::ParseTokens(line, {" ", " "}, timeRangeKey_.size());

      if (tokenList.size() >= 2)
      {
         using namespace std::chrono;

#if !(defined(_MSC_VER) || defined(__clang__))
         using namespace date;
#endif

         static const std::string dateTimeFormat {"%Y-%m-%dT%H:%M:%S"};

         std::istringstream ssStartTime {tokenList[0]};
         std::istringstream ssEndTime {tokenList[1]};

         std::chrono::sys_time<seconds> startTime;
         std::chrono::sys_time<seconds> endTime;

         ssStartTime >> parse(dateTimeFormat, startTime);
         ssEndTime >> parse(dateTimeFormat, endTime);

         if (!ssStartTime.fail() && !ssEndTime.fail())
         {
            startTime_ = startTime;
            endTime_   = endTime;
         }
         else
         {
            startTime_ = {};
            endTime_   = {};

            logger_->warn("TimeRange statement parse error: {}", line);
         }
      }
      else
      {
         logger_->warn("TimeRange statement malformed: {}", line);
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
   else if (boost::istarts_with(line, scwxModulateIconKey_))
   {
      // Supercell Wx Extension
      // scwx-ModulateIcon: red green blue [alpha]
      std::vector<std::string> tokenList = util::ParseTokens(
         line, {" ", " ", " ", " "}, scwxModulateIconKey_.size());

      if (tokenList.size() >= 3)
      {
         iconModulate_ = ParseColor(tokenList, 0, colorMode_);
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
         di->startTime_ = startTime_;
         di->endTime_   = endTime_;

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
      std::vector<std::string> tokenList = util::ParseTokens(
         line, {",", ",", ",", ",", ","}, iconFileKey_.size());

      if (tokenList.size() >= 6)
      {
         std::shared_ptr<IconFile> iconFile = std::make_shared<IconFile>();

         iconFile->fileNumber_ = std::stoul(tokenList[0]);
         iconFile->iconWidth_  = std::stoul(tokenList[1]);
         iconFile->iconHeight_ = std::stoul(tokenList[2]);
         iconFile->hotX_       = std::stoul(tokenList[3]);
         iconFile->hotY_       = std::stoul(tokenList[4]);

         TrimQuotes(tokenList[5]);
         iconFile->filename_.swap(tokenList[5]);

         iconFiles_.insert_or_assign(iconFile->fileNumber_, iconFile);
      }
      else
      {
         logger_->warn("IconFile statement malformed: {}", line);
      }
   }
   else if (boost::istarts_with(line, iconKey_))
   {
      // Icon: lat, lon, angle, fileNumber, iconNumber, hoverText
      std::vector<std::string> tokenList =
         util::ParseTokens(line, {",", ",", ",", ",", ","}, iconKey_.size());

      std::shared_ptr<IconDrawItem> di = nullptr;

      if (tokenList.size() >= 5)
      {
         di = std::make_shared<IconDrawItem>();

         di->threshold_ = threshold_;
         di->startTime_ = startTime_;
         di->endTime_   = endTime_;
         di->modulate_  = iconModulate_;

         ParseLocation(tokenList[0],
                       tokenList[1],
                       di->latitude_,
                       di->longitude_,
                       di->x_,
                       di->y_);

         di->angle_ = units::angle::degrees<double>(std::stod(tokenList[2]));

         di->fileNumber_ = std::stoul(tokenList[3]);
         di->iconNumber_ = std::stoul(tokenList[4]);
      }
      if (tokenList.size() >= 6)
      {
         ProcessEscapeCharacters(tokenList[5]);
         TrimQuotes(tokenList[5]);
         di->hoverText_.swap(tokenList[5]);
      }

      if (di != nullptr)
      {
         drawItems_.emplace_back(std::move(di));
      }
      else
      {
         logger_->warn("Icon statement malformed: {}", line);
      }
   }
   else if (boost::istarts_with(line, fontKey_))
   {
      // Font: fontNumber, pixels, flags, "face"
      std::vector<std::string> tokenList =
         util::ParseTokens(line, {",", ",", ",", ","}, fontKey_.size());

      if (tokenList.size() >= 4)
      {
         std::shared_ptr<Font> font = std::make_shared<Font>();

         font->fontNumber_ = std::stoul(tokenList[0]);
         font->pixels_     = std::stoul(tokenList[1]);
         font->flags_      = std::stoi(tokenList[2]);

         TrimQuotes(tokenList[3]);
         font->face_.swap(tokenList[3]);

         fonts_.insert_or_assign(font->fontNumber_, font);
      }
      else
      {
         logger_->warn("Font statement malformed: {}", line);
      }
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
         di->startTime_ = startTime_;
         di->endTime_   = endTime_;

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
   }
   else if (boost::istarts_with(line, lineKey_))
   {
      // Line: width, flags [, hover_text]
      //    lat, lon
      //    ...
      // End:
      std::vector<std::string> tokenList =
         util::ParseTokens(line, {",", ","}, lineKey_.size());

      currentStatement_ = DrawingStatement::Line;

      std::shared_ptr<LineDrawItem> di = nullptr;

      if (tokenList.size() >= 2)
      {
         di = std::make_shared<LineDrawItem>();

         di->threshold_ = threshold_;
         di->color_     = color_;
         di->startTime_ = startTime_;
         di->endTime_   = endTime_;

         di->width_ = std::stoul(tokenList[0]);

         if (!tokenList[1].empty())
         {
            di->flags_ = std::stoul(tokenList[1]);
         }
      }
      if (tokenList.size() >= 3)
      {
         ProcessEscapeCharacters(tokenList[2]);
         TrimQuotes(tokenList[2]);
         di->hoverText_.swap(tokenList[2]);
      }

      if (di != nullptr)
      {
         currentDrawItem_ = di;
         drawItems_.emplace_back(std::move(di));
      }
      else
      {
         logger_->warn("Line statement malformed: {}", line);
      }
   }
   else if (boost::istarts_with(line, trianglesKey_))
   {
      // Triangles:
      //    lat, lon [, r, g, b [,a]]
      //    ...
      // End:
      currentStatement_ = DrawingStatement::Triangles;

      std::shared_ptr<TrianglesDrawItem> di =
         std::make_shared<TrianglesDrawItem>();

      di->threshold_ = threshold_;
      di->color_     = color_;
      di->startTime_ = startTime_;
      di->endTime_   = endTime_;

      currentDrawItem_ = di;
      drawItems_.emplace_back(std::move(di));
   }
   else if (boost::istarts_with(line, imageKey_))
   {
      // Image: image_file
      //    lat, lon, Tu [, Tv ]
      //    ...
      // End:
      std::vector<std::string> tokenList =
         util::ParseTokens(line, {" "}, imageKey_.size());

      currentStatement_ = DrawingStatement::Image;

      std::shared_ptr<ImageDrawItem> di = nullptr;

      if (tokenList.size() >= 1)
      {
         di = std::make_shared<ImageDrawItem>();

         di->threshold_ = threshold_;
         di->startTime_ = startTime_;
         di->endTime_   = endTime_;

         TrimQuotes(tokenList[0]);
         di->imageFile_.swap(tokenList[0]);

         currentDrawItem_ = di;
         drawItems_.emplace_back(std::move(di));
      }
      else
      {
         logger_->warn("Image statement malformed: {}", line);
      }
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

      std::shared_ptr<PolygonDrawItem> di = std::make_shared<PolygonDrawItem>();

      di->threshold_ = threshold_;
      di->color_     = color_;
      di->startTime_ = startTime_;
      di->endTime_   = endTime_;

      currentDrawItem_ = di;
      drawItems_.emplace_back(std::move(di));
   }
   else
   {
      logger_->trace("Unknown statement: {}", line);
   }
}

void Placefile::Impl::ProcessElement(const std::string& line)
{
   if (currentStatement_ == DrawingStatement::Line)
   {
      // Line: width, flags [, hover_text]
      //    lat, lon
      //    ...
      // End:
      std::vector<std::string> tokenList = util::ParseTokens(line, {",", ","});

      if (tokenList.size() >= 2)
      {
         LineDrawItem::Element element;

         ParseLocation(tokenList[0],
                       tokenList[1],
                       element.latitude_,
                       element.longitude_,
                       element.x_,
                       element.y_);

         std::static_pointer_cast<LineDrawItem>(currentDrawItem_)
            ->elements_.emplace_back(std::move(element));
      }
      else
      {
         logger_->warn("Line sub-statement malformed: {}", line);
      }
   }
   else if (currentStatement_ == DrawingStatement::Triangles)
   {
      // Triangles:
      //    lat, lon [, r, g, b [,a]]
      //    ...
      // End:
      std::vector<std::string> tokenList =
         util::ParseTokens(line, {",", ",", ",", ",", ",", ","});

      TrianglesDrawItem::Element element;

      if (tokenList.size() >= 5)
      {
         element.color_ = ParseColor(tokenList, 2, colorMode_);
      }

      if (tokenList.size() >= 2)
      {
         ParseLocation(tokenList[0],
                       tokenList[1],
                       element.latitude_,
                       element.longitude_,
                       element.x_,
                       element.y_);

         std::static_pointer_cast<TrianglesDrawItem>(currentDrawItem_)
            ->elements_.emplace_back(std::move(element));
      }
      else
      {
         logger_->warn("Triangles sub-statement malformed: {}", line);
      }
   }
   else if (currentStatement_ == DrawingStatement::Image)
   {
      // Image: image_file
      //    lat, lon, Tu [, Tv ]
      //    ...
      // End:
      std::vector<std::string> tokenList =
         util::ParseTokens(line, {",", ",", ",", ","});

      ImageDrawItem::Element element;

      if (tokenList.size() >= 3)
      {
         ParseLocation(tokenList[0],
                       tokenList[1],
                       element.latitude_,
                       element.longitude_,
                       element.x_,
                       element.y_);

         element.tu_ = std::stod(tokenList[2]);
      }

      if (tokenList.size() >= 4)
      {
         element.tv_ = std::stod(tokenList[3]);
      }
      else
      {
         element.tv_ = element.tu_;
      }

      if (tokenList.size() >= 3)
      {
         std::static_pointer_cast<ImageDrawItem>(currentDrawItem_)
            ->elements_.emplace_back(std::move(element));
      }
      else
      {
         logger_->warn("Image sub-statement malformed: {}", line);
      }
   }
   else if (currentStatement_ == DrawingStatement::Polygon)
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
      std::vector<std::string> tokenList =
         util::ParseTokens(line, {",", ",", ",", ",", ",", ","});

      PolygonDrawItem::Element element;

      if (tokenList.size() >= 5)
      {
         element.color_ = ParseColor(tokenList, 2, colorMode_);
      }

      if (tokenList.size() >= 2)
      {
         ParseLocation(tokenList[0],
                       tokenList[1],
                       element.latitude_,
                       element.longitude_,
                       element.x_,
                       element.y_);

         currentPolygonContour_.emplace_back(std::move(element));

         if (currentPolygonContour_.size() >= 2)
         {
            auto& first = currentPolygonContour_.front();
            auto& last  = currentPolygonContour_.back();

            // Repeating the first point closes the contour
            if (first.latitude_ == last.latitude_ &&
                first.longitude_ == last.longitude_ && //
                first.x_ == last.x_ &&                 //
                first.y_ == last.y_)
            {
               auto& contours =
                  std::static_pointer_cast<PolygonDrawItem>(currentDrawItem_)
                     ->contours_;

               auto& newContour = contours.emplace_back(
                  std::vector<PolygonDrawItem::Element> {});
               newContour.swap(currentPolygonContour_);
            }
         }
      }
      else
      {
         logger_->warn("Polygon sub-statement malformed: {}", line);
      }
   }
}

void Placefile::Impl::ProcessElementEnd()
{
   if (currentStatement_ == DrawingStatement::Polygon)
   {
      auto di = std::static_pointer_cast<PolygonDrawItem>(currentDrawItem_);

      // Complete the current contour when ending the Polygon statement
      if (!currentPolygonContour_.empty())
      {
         auto& contours = di->contours_;

         auto& newContour =
            contours.emplace_back(std::vector<PolygonDrawItem::Element> {});
         newContour.swap(currentPolygonContour_);
      }

      if (!di->contours_.empty())
      {
         std::vector<common::Coordinate> coordinates {};
         std::transform(di->contours_[0].cbegin(),
                        di->contours_[0].cend(),
                        std::back_inserter(coordinates),
                        [](auto& element) {
                           return common::Coordinate {element.latitude_,
                                                      element.longitude_};
                        });
         di->center_ = GetCentroid(coordinates);
      }
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
   boost::replace_all(s, "\\r", "\r");
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
