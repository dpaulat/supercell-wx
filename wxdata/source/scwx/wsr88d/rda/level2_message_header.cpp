#include <scwx/wsr88d/rda/level2_message_header.hpp>
#include <scwx/util/logger.hpp>

#include <istream>
#include <string>

#ifdef _WIN32
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
   "scwx::wsr88d::rda::level2_message_header";
static const auto logger_ = util::Logger::Create(logPrefix_);

class Level2MessageHeaderImpl
{
public:
   explicit Level2MessageHeaderImpl() :
       messageSize_(),
       rdaRedundantChannel_(),
       messageType_(),
       idSequenceNumber_(),
       julianDate_(),
       millisecondsOfDay_(),
       numberOfMessageSegments_(),
       messageSegmentNumber_() {};
   ~Level2MessageHeaderImpl() = default;

   uint16_t messageSize_;
   uint8_t  rdaRedundantChannel_;
   uint8_t  messageType_;
   uint16_t idSequenceNumber_;
   uint16_t julianDate_;
   uint32_t millisecondsOfDay_;
   uint16_t numberOfMessageSegments_;
   uint16_t messageSegmentNumber_;
};

Level2MessageHeader::Level2MessageHeader() :
    p(std::make_unique<Level2MessageHeaderImpl>())
{
}
Level2MessageHeader::~Level2MessageHeader() = default;

Level2MessageHeader::Level2MessageHeader(Level2MessageHeader&&) noexcept =
   default;
Level2MessageHeader&
Level2MessageHeader::operator=(Level2MessageHeader&&) noexcept = default;

uint16_t Level2MessageHeader::message_size() const
{
   return p->messageSize_;
}

uint8_t Level2MessageHeader::rda_redundant_channel() const
{
   return p->rdaRedundantChannel_;
}

uint8_t Level2MessageHeader::message_type() const
{
   return p->messageType_;
}

uint16_t Level2MessageHeader::id_sequence_number() const
{
   return p->idSequenceNumber_;
}

uint16_t Level2MessageHeader::julian_date() const
{
   return p->julianDate_;
}

uint32_t Level2MessageHeader::milliseconds_of_day() const
{
   return p->millisecondsOfDay_;
}

uint16_t Level2MessageHeader::number_of_message_segments() const
{
   return p->numberOfMessageSegments_;
}

uint16_t Level2MessageHeader::message_segment_number() const
{
   return p->messageSegmentNumber_;
}

void Level2MessageHeader::set_message_size(uint16_t messageSize)
{
   p->messageSize_ = messageSize;
}

bool Level2MessageHeader::Parse(std::istream& is)
{
   bool headerValid = true;

   is.read(reinterpret_cast<char*>(&p->messageSize_), 2);
   is.read(reinterpret_cast<char*>(&p->rdaRedundantChannel_), 1);
   is.read(reinterpret_cast<char*>(&p->messageType_), 1);
   is.read(reinterpret_cast<char*>(&p->idSequenceNumber_), 2);
   is.read(reinterpret_cast<char*>(&p->julianDate_), 2);
   is.read(reinterpret_cast<char*>(&p->millisecondsOfDay_), 4);
   is.read(reinterpret_cast<char*>(&p->numberOfMessageSegments_), 2);
   is.read(reinterpret_cast<char*>(&p->messageSegmentNumber_), 2);

   p->messageSize_             = ntohs(p->messageSize_);
   p->idSequenceNumber_        = ntohs(p->idSequenceNumber_);
   p->julianDate_              = ntohs(p->julianDate_);
   p->millisecondsOfDay_       = ntohl(p->millisecondsOfDay_);
   p->numberOfMessageSegments_ = ntohs(p->numberOfMessageSegments_);
   p->messageSegmentNumber_    = ntohs(p->messageSegmentNumber_);

   if (is.eof())
   {
      logger_->debug("Reached end of file");
      headerValid = false;
   }
   else
   {
      if (p->messageSize_ < 9)
      {
         if (p->messageSize_ != 0)
         {
            logger_->warn("Invalid message size: {}", p->messageSize_);
         }
         headerValid = false;
      }
      if (p->millisecondsOfDay_ > 86'399'999u)
      {
         logger_->warn("Invalid milliseconds: {}", p->millisecondsOfDay_);
         headerValid = false;
      }
      if (p->messageSize_ < 65534 &&
          p->messageSegmentNumber_ > p->numberOfMessageSegments_)
      {
         logger_->warn("Invalid segment = {}/{}",
                       p->messageSegmentNumber_,
                       p->numberOfMessageSegments_);
         headerValid = false;
      }
   }

   if (headerValid)
   {
      logger_->trace("Message type: {}",
                     static_cast<unsigned>(p->messageType_));
   }

   return headerValid;
}

} // namespace rda
} // namespace wsr88d
} // namespace scwx
