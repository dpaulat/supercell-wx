#include <scwx/wsr88d/rda/message_header.hpp>

#include <istream>
#include <string>

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

static const std::string logPrefix_ = "[scwx::wsr88d::rda::message_header] ";

class MessageHeaderImpl
{
public:
   explicit MessageHeaderImpl() :
       messageSize_(),
       rdaRedundantChannel_(),
       messageType_(),
       idSequenceNumber_(),
       julianDate_(),
       millisecondsOfDay_(),
       numberOfMessageSegments_(),
       messageSegmentNumber_() {};
   ~MessageHeaderImpl() = default;

   uint16_t messageSize_;
   uint8_t  rdaRedundantChannel_;
   uint8_t  messageType_;
   uint16_t idSequenceNumber_;
   uint16_t julianDate_;
   uint32_t millisecondsOfDay_;
   uint16_t numberOfMessageSegments_;
   uint16_t messageSegmentNumber_;
};

MessageHeader::MessageHeader() : p(std::make_unique<MessageHeaderImpl>()) {}
MessageHeader::~MessageHeader() = default;

MessageHeader::MessageHeader(MessageHeader&&) noexcept = default;
MessageHeader& MessageHeader::operator=(MessageHeader&&) noexcept = default;

uint16_t MessageHeader::message_size() const
{
   return p->messageSize_;
}

uint8_t MessageHeader::rda_redundant_channel() const
{
   return p->rdaRedundantChannel_;
}

uint8_t MessageHeader::message_type() const
{
   return p->messageType_;
}

uint16_t MessageHeader::id_sequence_number() const
{
   return p->idSequenceNumber_;
}

uint16_t MessageHeader::julian_date() const
{
   return p->julianDate_;
}

uint32_t MessageHeader::milliseconds_of_day() const
{
   return p->millisecondsOfDay_;
}

uint16_t MessageHeader::number_of_message_segments() const
{
   return p->numberOfMessageSegments_;
}

uint16_t MessageHeader::message_segment_number() const
{
   return p->messageSegmentNumber_;
}

void MessageHeader::set_message_size(uint16_t messageSize)
{
   p->messageSize_ = messageSize;
}

bool MessageHeader::Parse(std::istream& is)
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

   p->messageSize_             = htons(p->messageSize_);
   p->idSequenceNumber_        = htons(p->idSequenceNumber_);
   p->julianDate_              = htons(p->julianDate_);
   p->millisecondsOfDay_       = htonl(p->millisecondsOfDay_);
   p->numberOfMessageSegments_ = htons(p->numberOfMessageSegments_);
   p->messageSegmentNumber_    = htons(p->messageSegmentNumber_);

   if (is.eof())
   {
      BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Reached end of file";
      headerValid = false;
   }
   else
   {
      if (p->messageSize_ < 9)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid message size: " << p->messageSize_;
         headerValid = false;
      }
      if (p->julianDate_ < 1)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid date: " << p->julianDate_;
         headerValid = false;
      }
      if (p->millisecondsOfDay_ > 86'399'999u)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid milliseconds: " << p->millisecondsOfDay_;
         headerValid = false;
      }
      if (p->messageSize_ < 65534 &&
          p->messageSegmentNumber_ > p->numberOfMessageSegments_)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid segment = " << p->messageSegmentNumber_
            << "/" << p->numberOfMessageSegments_;
         headerValid = false;
      }
   }

   if (headerValid)
   {
      BOOST_LOG_TRIVIAL(trace)
         << logPrefix_
         << "Message type: " << static_cast<unsigned>(p->messageType_);
   }

   return headerValid;
}

} // namespace rda
} // namespace wsr88d
} // namespace scwx
