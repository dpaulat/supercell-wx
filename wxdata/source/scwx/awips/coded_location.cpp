#include <scwx/awips/coded_location.hpp>
#include <scwx/util/logger.hpp>

#include <sstream>

namespace scwx
{
namespace awips
{

static const std::string logPrefix_ = "scwx::awips::coded_location";
static const auto        logger_    = util::Logger::Create(logPrefix_);

class CodedLocationImpl
{
public:
   explicit CodedLocationImpl() : coordinates_ {} {}

   ~CodedLocationImpl() {}

   std::vector<common::Coordinate> coordinates_;
};

CodedLocation::CodedLocation() : p(std::make_unique<CodedLocationImpl>()) {}
CodedLocation::~CodedLocation() = default;

CodedLocation::CodedLocation(CodedLocation&&) noexcept = default;
CodedLocation& CodedLocation::operator=(CodedLocation&&) noexcept = default;

std::vector<common::Coordinate> CodedLocation::coordinates() const
{
   return p->coordinates_;
}

bool CodedLocation::Parse(const StringRange& lines, const std::string& wfo)
{
   enum class LocationFormat
   {
      WFO,
      NationalCenter
   };

   bool           dataValid = true;
   LocationFormat format {};

   std::vector<std::string> tokenList;

   for (std::string line : lines)
   {
      std::string        token;
      std::istringstream tokenStream {line};

      while (tokenStream >> token)
      {
         tokenList.push_back(token);
      }
   }

   // First token is "LAT...LON"
   // At a minimum, three points (latitude/longitude pairs) will be included
   dataValid = (tokenList.size() >= 4 && tokenList.at(0) == "LAT...LON");

   if (dataValid)
   {
      format = (tokenList.at(1).size() == 8) ? LocationFormat::NationalCenter :
                                               LocationFormat::WFO;

      if (format == LocationFormat::WFO)
      {
         dataValid = (tokenList.size() >= 7 && tokenList.size() % 2 == 1);
      }
   }

   if (dataValid)
   {
      if (format == LocationFormat::WFO)
      {
         const bool wfoIsWest         = (wfo != "PGUM");
         double     westLongitude     = (wfoIsWest) ? -1.0 : 1.0;
         bool       straddlesDateLine = false;

         for (auto token = tokenList.cbegin() + 1; token != tokenList.cend();
              ++token)
         {
            double latitude  = 0.0;
            double longitude = 0.0;

            try
            {
               latitude = std::stod(*token) * 0.01;
               ++token;
               longitude = std::stod(*token) * 0.01;
            }
            catch (const std::exception& ex)
            {
               logger_->warn(
                  "Invalid WFO location token: \"{}\" ({})", *token, ex.what());
               dataValid = false;
               break;
            }

            // If a given product straddles 180 degrees longitude, those points
            // west of 180 degrees will be given as if they were west longitude
            if (longitude > 180.0)
            {
               longitude -= 360.0;
               straddlesDateLine = true;
            }

            longitude *= westLongitude;

            p->coordinates_.push_back({latitude, longitude});
         }

         if (dataValid && !wfoIsWest && straddlesDateLine)
         {
            for (auto& coordinate : p->coordinates_)
            {
               coordinate.longitude_ *= -1.0;
            }
         }
      }
      else
      {
         for (auto token = tokenList.cbegin() + 1; token != tokenList.cend();
              ++token)
         {
            if (token->size() != 8)
            {
               logger_->warn("Invalid National Center LAT...LON format: \"{}\"",
                             *token);

               dataValid = false;
               break;
            }

            double latitude  = 0.0;
            double longitude = 0.0;

            try
            {
               latitude  = std::stod(token->substr(0, 4)) * 0.01;
               longitude = std::stod(token->substr(4, 4)) * -0.01;
            }
            catch (const std::exception& ex)
            {
               logger_->warn(
                  "Invalid National Center location token: \"{}\" ({})",
                  *token,
                  ex.what());
               dataValid = false;
               break;
            }

            // Longitudes of greater than 100 degrees will drop the leading 1;
            // i.e., 105.22 W would be coded as 0522.  This is ambiguous
            // with 5.22 W, so we assume everything east of 65 W (east of Maine,
            // easternmost point of CONUS) should have 100 degrees added to it.
            // Points in the Atlantic or western Alaska will not be correct, but
            // it is assumed that products will not contain those points coded
            // using this methodology.
            if (longitude > -65.0)
            {
               longitude -= 100.0;
            }

            p->coordinates_.push_back({latitude, longitude});
         }
      }
   }
   else
   {
      if (tokenList.empty())
      {
         logger_->warn("LAT...LON not found");
      }
      else
      {
         logger_->warn("Malformed LAT...LON tokens: (0: {}, size: {})",
                       tokenList.at(0),
                       tokenList.size());

         for (const auto& token : tokenList)
         {
            logger_->debug("{}", token);
         }
      }

      dataValid = false;
   }

   if (dataValid)
   {
      // If the last point is a repeat of the first point, remove it as
      // redundant
      if (p->coordinates_.front() == p->coordinates_.back())
      {
         p->coordinates_.pop_back();
      }
   }

   return dataValid;
}

std::optional<CodedLocation> CodedLocation::Create(const StringRange& lines,
                                                   const std::string& wfo)
{
   std::optional<CodedLocation> location = std::make_optional<CodedLocation>();

   if (!location->Parse(lines, wfo))
   {
      location.reset();
   }

   return location;
}

} // namespace awips
} // namespace scwx
