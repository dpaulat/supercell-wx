#include <scwx/wsr88d/rda/clutter_filter_map.hpp>

#include <istream>
#include <vector>

#include <boost/log/trivial.hpp>

#ifdef WIN32
#   include <WinSock2.h>
#else
#   include <arpa/inet.h>
#endif

namespace scwx
{
namespace wsr88d
{
namespace rda
{

static const std::string logPrefix_ =
   "[scwx::wsr88d::rda::clutter_filter_map] ";

struct RangeZone
{
   uint16_t opCode;
   uint16_t endRange;
};

class ClutterFilterMapImpl
{
public:
   explicit ClutterFilterMapImpl() :
       mapGenerationDate_(), mapGenerationTime_(), rangeZones_() {};
   ~ClutterFilterMapImpl() = default;

   uint16_t mapGenerationDate_;
   uint16_t mapGenerationTime_;

   std::vector<std::vector<std::vector<RangeZone>>> rangeZones_;
};

ClutterFilterMap::ClutterFilterMap() :
    Message(), p(std::make_unique<ClutterFilterMapImpl>())
{
}
ClutterFilterMap::~ClutterFilterMap() = default;

ClutterFilterMap::ClutterFilterMap(ClutterFilterMap&&) noexcept = default;
ClutterFilterMap&
ClutterFilterMap::operator=(ClutterFilterMap&&) noexcept = default;

uint16_t ClutterFilterMap::map_generation_date() const
{
   return p->mapGenerationDate_;
}

uint16_t ClutterFilterMap::map_generation_time() const
{
   return p->mapGenerationTime_;
}

uint16_t ClutterFilterMap::number_of_elevation_segments() const
{
   return static_cast<uint16_t>(p->rangeZones_.size());
}

uint16_t ClutterFilterMap::number_of_range_zones(uint16_t e, uint16_t a) const
{
   return static_cast<uint16_t>(p->rangeZones_[e][a].size());
}

uint16_t ClutterFilterMap::op_code(uint16_t e, uint16_t a, uint16_t z) const
{
   return p->rangeZones_[e][a][z].opCode;
}

uint16_t ClutterFilterMap::end_range(uint16_t e, uint16_t a, uint16_t z) const
{
   return p->rangeZones_[e][a][z].endRange;
}

bool ClutterFilterMap::Parse(std::istream& is)
{
   BOOST_LOG_TRIVIAL(debug)
      << logPrefix_ << "Parsing Clutter Filter Map (Message Type 15)";

   bool     messageValid         = true;
   size_t   bytesRead            = 0;
   uint16_t numElevationSegments = 0;

   is.read(reinterpret_cast<char*>(&p->mapGenerationDate_), 2);
   is.read(reinterpret_cast<char*>(&p->mapGenerationTime_), 2);
   is.read(reinterpret_cast<char*>(&numElevationSegments), 2);
   bytesRead += 6;

   p->mapGenerationDate_ = htons(p->mapGenerationDate_);
   p->mapGenerationTime_ = htons(p->mapGenerationTime_);
   numElevationSegments  = htons(numElevationSegments);

   if (is.eof())
   {
      BOOST_LOG_TRIVIAL(warning) << logPrefix_ << "Reached end of file (1)";
      messageValid = false;
   }
   else
   {
      if (p->mapGenerationDate_ < 1)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid date: " << p->mapGenerationDate_;
         messageValid = false;
      }
      if (p->mapGenerationTime_ > 1440)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid time: " << p->mapGenerationTime_;
         messageValid = false;
      }
      if (numElevationSegments < 1 || numElevationSegments > 5)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_
            << "Invalid number of elevation segments: " << numElevationSegments;
         messageValid = false;
      }
   }

   if (!messageValid)
   {
      numElevationSegments = 0;
   }

   p->rangeZones_.resize(numElevationSegments);

   for (uint16_t e = 0; e < numElevationSegments && messageValid; e++)
   {
      p->rangeZones_[e].resize(NUM_AZIMUTH_SEGMENTS);

      for (uint16_t a = 0; a < NUM_AZIMUTH_SEGMENTS && messageValid; a++)
      {
         uint16_t numRangeZones;
         is.read(reinterpret_cast<char*>(&numRangeZones), 2);
         bytesRead += 2;

         numRangeZones = htons(numRangeZones);

         if (is.eof())
         {
            BOOST_LOG_TRIVIAL(warning)
               << logPrefix_ << "Reached end of file (2)";
            messageValid = false;
         }
         else
         {
            if (numRangeZones < 1 || numRangeZones > 20)
            {
               BOOST_LOG_TRIVIAL(warning)
                  << logPrefix_
                  << "Invalid number of range zones: " << numRangeZones;
               messageValid = false;
            }
         }

         if (!messageValid)
         {
            break;
         }

         p->rangeZones_[e][a].resize(numRangeZones);

         for (uint16_t z = 0; z < numRangeZones && messageValid; z++)
         {
            RangeZone& zone = p->rangeZones_[e][a][z];

            is.read(reinterpret_cast<char*>(&zone.opCode), 2);
            is.read(reinterpret_cast<char*>(&zone.endRange), 2);
            bytesRead += 4;

            zone.opCode   = htons(zone.opCode);
            zone.endRange = htons(zone.endRange);

            if (is.eof())
            {
               BOOST_LOG_TRIVIAL(warning)
                  << logPrefix_ << "Reached end of file (3)";
               messageValid = false;
            }
            else
            {
               if (zone.opCode > 2)
               {
                  BOOST_LOG_TRIVIAL(warning)
                     << logPrefix_ << "Invalid op code: " << zone.opCode;
                  messageValid = false;
               }
               if (zone.endRange > 511)
               {
                  BOOST_LOG_TRIVIAL(warning)
                     << logPrefix_ << "Invalid end range: " << zone.endRange;
                  messageValid = false;
               }
            }
         }
      }
   }

   if (!ValidateSize(is, bytesRead))
   {
      messageValid = false;
   }

   if (!messageValid)
   {
      p->rangeZones_.resize(0);
      p->rangeZones_.shrink_to_fit();
   }

   return messageValid;
}

std::unique_ptr<ClutterFilterMap>
ClutterFilterMap::Create(MessageHeader&& header, std::istream& is)
{
   std::unique_ptr<ClutterFilterMap> message =
      std::make_unique<ClutterFilterMap>();
   message->set_header(std::move(header));
   message->Parse(is);
   return message;
}

} // namespace rda
} // namespace wsr88d
} // namespace scwx
