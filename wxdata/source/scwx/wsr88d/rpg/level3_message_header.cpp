#include <scwx/wsr88d/rpg/level3_message_header.hpp>
#include <scwx/util/logger.hpp>

#include <istream>
#include <string>

#ifdef WIN32
#   include <WinSock2.h>
#else
#   include <arpa/inet.h>
#endif

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ =
   "scwx::wsr88d::rpg::level3_message_header";
static const auto logger_ = util::Logger::Create(logPrefix_);

class Level3MessageHeaderImpl
{
public:
   explicit Level3MessageHeaderImpl() :
       messageCode_ {0},
       dateOfMessage_ {0},
       timeOfMessage_ {0},
       lengthOfMessage_ {0},
       sourceId_ {0},
       destinationId_ {0},
       numberBlocks_ {0}
   {
   }
   ~Level3MessageHeaderImpl() = default;

   int16_t  messageCode_;
   uint16_t dateOfMessage_;
   uint32_t timeOfMessage_;
   uint32_t lengthOfMessage_;
   uint16_t sourceId_;
   uint16_t destinationId_;
   uint16_t numberBlocks_;
};

Level3MessageHeader::Level3MessageHeader() :
    p(std::make_unique<Level3MessageHeaderImpl>())
{
}
Level3MessageHeader::~Level3MessageHeader() = default;

Level3MessageHeader::Level3MessageHeader(Level3MessageHeader&&) noexcept =
   default;
Level3MessageHeader&
Level3MessageHeader::operator=(Level3MessageHeader&&) noexcept = default;

int16_t Level3MessageHeader::message_code() const
{
   return p->messageCode_;
}

uint16_t Level3MessageHeader::date_of_message() const
{
   return p->dateOfMessage_;
}

uint32_t Level3MessageHeader::time_of_message() const
{
   return p->timeOfMessage_;
}

uint32_t Level3MessageHeader::length_of_message() const
{
   return p->lengthOfMessage_;
}

uint16_t Level3MessageHeader::source_id() const
{
   return p->sourceId_;
}

uint16_t Level3MessageHeader::destination_id() const
{
   return p->destinationId_;
}

uint16_t Level3MessageHeader::number_blocks() const
{
   return p->numberBlocks_;
}

bool Level3MessageHeader::Parse(std::istream& is)
{
   bool headerValid = true;

   is.read(reinterpret_cast<char*>(&p->messageCode_), 2);
   is.read(reinterpret_cast<char*>(&p->dateOfMessage_), 2);
   is.read(reinterpret_cast<char*>(&p->timeOfMessage_), 4);
   is.read(reinterpret_cast<char*>(&p->lengthOfMessage_), 4);
   is.read(reinterpret_cast<char*>(&p->sourceId_), 2);
   is.read(reinterpret_cast<char*>(&p->destinationId_), 2);
   is.read(reinterpret_cast<char*>(&p->numberBlocks_), 2);

   p->messageCode_     = ntohs(p->messageCode_);
   p->dateOfMessage_   = ntohs(p->dateOfMessage_);
   p->timeOfMessage_   = ntohl(p->timeOfMessage_);
   p->lengthOfMessage_ = ntohl(p->lengthOfMessage_);
   p->sourceId_        = ntohs(p->sourceId_);
   p->destinationId_   = ntohs(p->destinationId_);
   p->numberBlocks_    = ntohs(p->numberBlocks_);

   if (is.eof())
   {
      logger_->debug("Reached end of file");
      headerValid = false;
   }
   else
   {
      if (p->messageCode_ < -131 ||
          (p->messageCode_ > -16 && p->messageCode_ < 0) ||
          p->messageCode_ > 211)
      {
         logger_->warn("Invalid message code: {}", p->messageCode_);
         headerValid = false;
      }
      if (p->dateOfMessage_ > 32'767u)
      {
         logger_->warn("Invalid date: {}", p->dateOfMessage_);
         headerValid = false;
      }
      if (p->timeOfMessage_ > 86'399u)
      {
         logger_->warn("Invalid time: {}", p->timeOfMessage_);
         headerValid = false;
      }
      if (p->lengthOfMessage_ < 18 || p->lengthOfMessage_ > 1'329'270u)
      {
         logger_->warn("Invalid length: {}", p->lengthOfMessage_);
         headerValid = false;
      }
      if ((p->sourceId_ > 999u && p->sourceId_ < 3000) || p->sourceId_ > 3045)
      {
         logger_->warn("Invalid source ID: {}", p->sourceId_);
         headerValid = false;
      }
      if (p->destinationId_ > 999u)
      {
         logger_->warn("Invalid destination ID: {}", p->destinationId_);
         headerValid = false;
      }
      if (p->numberBlocks_ < 1u || p->numberBlocks_ > 51u)
      {
         logger_->warn("Invalid block count: {}", p->numberBlocks_);
         headerValid = false;
      }
   }

   if (headerValid)
   {
      logger_->trace("Message code: {}", p->messageCode_);
   }

   return headerValid;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
