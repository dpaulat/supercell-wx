#include <scwx/wsr88d/rda/clutter_filter_bypass_map.hpp>
#include <scwx/util/logger.hpp>

#include <vector>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

static const std::string logPrefix_ =
   "scwx::wsr88d::rda::clutter_filter_bypass_map";
static const auto logger_ = util::Logger::Create(logPrefix_);

class ClutterFilterBypassMapImpl
{
public:
   explicit ClutterFilterBypassMapImpl() :
       mapGenerationDate_(), mapGenerationTime_(), rangeBins_() {};
   ~ClutterFilterBypassMapImpl() = default;

   uint16_t mapGenerationDate_;
   uint16_t mapGenerationTime_;

   std::vector<std::vector<std::vector<uint16_t>>> rangeBins_;
};

ClutterFilterBypassMap::ClutterFilterBypassMap() :
    Level2Message(), p(std::make_unique<ClutterFilterBypassMapImpl>())
{
}
ClutterFilterBypassMap::~ClutterFilterBypassMap() = default;

ClutterFilterBypassMap::ClutterFilterBypassMap(
   ClutterFilterBypassMap&&) noexcept = default;
ClutterFilterBypassMap&
ClutterFilterBypassMap::operator=(ClutterFilterBypassMap&&) noexcept = default;

uint16_t ClutterFilterBypassMap::map_generation_date() const
{
   return p->mapGenerationDate_;
}

uint16_t ClutterFilterBypassMap::map_generation_time() const
{
   return p->mapGenerationTime_;
}

uint16_t ClutterFilterBypassMap::number_of_elevation_segments() const
{
   return static_cast<uint16_t>(p->rangeBins_.size());
}

uint16_t
ClutterFilterBypassMap::range_bin(uint16_t e, uint16_t r, uint16_t b) const
{
   return p->rangeBins_[e][r][b];
}

bool ClutterFilterBypassMap::Parse(std::istream& is)
{
   logger_->trace("Parsing Clutter Filter Bypass Map (Message Type 13)");

   bool     messageValid         = true;
   size_t   bytesRead            = 0;
   uint16_t numElevationSegments = 0;

   is.read(reinterpret_cast<char*>(&p->mapGenerationDate_), 2);
   is.read(reinterpret_cast<char*>(&p->mapGenerationTime_), 2);
   is.read(reinterpret_cast<char*>(&numElevationSegments), 2);
   bytesRead += 6;

   p->mapGenerationDate_ = ntohs(p->mapGenerationDate_);
   p->mapGenerationTime_ = ntohs(p->mapGenerationTime_);
   numElevationSegments  = ntohs(numElevationSegments);

   if (p->mapGenerationDate_ < 1)
   {
      logger_->warn("Invalid date: {}", p->mapGenerationDate_);
      messageValid = false;
   }
   if (p->mapGenerationTime_ > 1440)
   {
      logger_->warn("Invalid time: {}", p->mapGenerationTime_);
      messageValid = false;
   }
   if (numElevationSegments < 1 || numElevationSegments > 5)
   {
      logger_->warn("Invalid number of elevation segments: {}",
                    numElevationSegments);
      messageValid = false;
   }

   if (!messageValid)
   {
      numElevationSegments = 0;
   }

   p->rangeBins_.resize(numElevationSegments);

   for (uint16_t e = 0; e < numElevationSegments && messageValid; e++)
   {
      p->rangeBins_[e].resize(NUM_RADIALS);

      is.seekg(2, std::ios_base::cur); // Segment number (redundant)

      for (uint16_t r = 0; r < NUM_RADIALS && messageValid; r++)
      {
         p->rangeBins_[e][r].resize(NUM_CODED_RANGE_BINS);

         is.read(reinterpret_cast<char*>(p->rangeBins_[e][r].data()),
                 NUM_RANGE_BINS / 8u);
         bytesRead += NUM_RANGE_BINS / 8u;

         SwapVector(p->rangeBins_[e][r]);
      }
   }

   if (!ValidateMessage(is, bytesRead))
   {
      messageValid = false;
   }

   if (!messageValid)
   {
      p->rangeBins_.resize(0);
      p->rangeBins_.shrink_to_fit();
   }

   return messageValid;
}

std::shared_ptr<ClutterFilterBypassMap>
ClutterFilterBypassMap::Create(Level2MessageHeader&& header, std::istream& is)
{
   std::shared_ptr<ClutterFilterBypassMap> message =
      std::make_shared<ClutterFilterBypassMap>();
   message->set_header(std::move(header));

   if (!message->Parse(is))
   {
      message.reset();
   }

   return message;
}

} // namespace rda
} // namespace wsr88d
} // namespace scwx
