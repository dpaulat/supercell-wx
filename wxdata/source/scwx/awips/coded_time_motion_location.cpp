// Enable chrono formatters
#ifndef __cpp_lib_format
#   define __cpp_lib_format 202110L
#endif

#include <scwx/awips/coded_time_motion_location.hpp>

#include <sstream>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace awips
{

static const std::string logPrefix_ =
   "[scwx::awips::coded_time_motion_location] ";

class CodedTimeMotionLocationImpl
{
public:
   explicit CodedTimeMotionLocationImpl() :
       time_ {}, direction_ {0}, speed_ {0}, coordinates_ {}
   {
   }

   ~CodedTimeMotionLocationImpl() {}

   std::chrono::hh_mm_ss<std::chrono::minutes> time_;
   uint16_t                                    direction_;
   uint8_t                                     speed_;
   std::vector<common::Coordinate>             coordinates_;
};

CodedTimeMotionLocation::CodedTimeMotionLocation() :
    p(std::make_unique<CodedTimeMotionLocationImpl>())
{
}
CodedTimeMotionLocation::~CodedTimeMotionLocation() = default;

CodedTimeMotionLocation::CodedTimeMotionLocation(
   CodedTimeMotionLocation&&) noexcept                    = default;
CodedTimeMotionLocation& CodedTimeMotionLocation::operator=(
   CodedTimeMotionLocation&&) noexcept = default;

std::chrono::hh_mm_ss<std::chrono::minutes>
CodedTimeMotionLocation::time() const
{
   return p->time_;
}

uint16_t CodedTimeMotionLocation::direction() const
{
   return p->direction_;
}

uint8_t CodedTimeMotionLocation::speed() const
{
   return p->speed_;
}

std::vector<common::Coordinate> CodedTimeMotionLocation::coordinates() const
{
   return p->coordinates_;
}

bool CodedTimeMotionLocation::Parse(const StringRange& lines,
                                    const std::string& wfo)
{
   bool dataValid = true;

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

   // First token is "TIME...MOT...LOC"
   // At a minimum, one point (latitude/longitude pair) will be included
   dataValid = (tokenList.size() >= 6 && tokenList.size() % 2 == 0 &&
                tokenList.at(0) == "TIME...MOT...LOC");

   if (dataValid)
   {
      const bool wfoIsWest         = (wfo != "PGUM");
      double     westLongitude     = (wfoIsWest) ? -1.0 : 1.0;
      bool       straddlesDateLine = false;

      // Time: hhmmZ
      std::string time = tokenList.at(1);
      {
         using namespace std::chrono;

         static const std::string timeFormat {"%H%MZ"};

         std::istringstream in {time};
         minutes            tp;
         in >> parse(timeFormat, tp);

         if (time.size() == 5 && !in.fail())
         {
            p->time_ = hh_mm_ss {tp};
         }
         else
         {
            BOOST_LOG_TRIVIAL(warning)
               << logPrefix_ << "Invalid time: \"" << time << "\"";
            p->time_  = hh_mm_ss<minutes> {};
            dataValid = false;
         }
      }

      // Direction: dirDEG
      std::string direction = tokenList.at(2);
      if (direction.size() == 6 && direction.ends_with("DEG"))
      {
         p->direction_ =
            static_cast<uint16_t>(std::stoul(direction.substr(0, 3)));
      }
      else
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid direction: \"" << direction << "\"";
         dataValid = false;
      }

      // Speed: <sp>KT
      std::string speed = tokenList.at(3);
      if (speed.size() >= 3 && speed.size() <= 4 && speed.ends_with("KT"))
      {
         p->speed_ =
            static_cast<uint8_t>(std::stoul(speed.substr(0, speed.size() - 2)));
      }
      else
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid speed: \"" << speed << "\"";
         dataValid = false;
      }

      // Location
      for (auto token = tokenList.cbegin() + 4; token != tokenList.cend();
           ++token)
      {
         double latitude = std::stod(*token) * 0.01;
         ++token;
         double longitude = std::stod(*token) * 0.01;

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

      if (!wfoIsWest && straddlesDateLine)
      {
         for (auto& coordinate : p->coordinates_)
         {
            coordinate.longitude_ *= -1.0;
         }
      }
   }

   return dataValid;
}

} // namespace awips
} // namespace scwx
