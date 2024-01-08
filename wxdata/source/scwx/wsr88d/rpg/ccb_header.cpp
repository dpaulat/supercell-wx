#include <scwx/wsr88d/rpg/ccb_header.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/streams.hpp>

#include <istream>
#include <sstream>
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
namespace rpg
{

static const std::string logPrefix_ = "scwx::wsr88d::rpg::ccb_header";
static const auto        logger_    = util::Logger::Create(logPrefix_);

class CcbHeaderImpl
{
public:
   explicit CcbHeaderImpl() :
       ff_ {0},
       ccbLength_ {0},
       mode_ {0},
       submode_ {0},
       precedence_ {0},
       classification_ {0},
       messageOriginator_ {},
       category_ {0},
       subcategory_ {0},
       userDefined_ {0},
       year_ {0},
       month_ {0},
       torDay_ {0},
       torHour_ {0},
       torMinute_ {0},
       numberOfDestinations_ {0},
       messageDestination_ {}
   {
   }
   ~CcbHeaderImpl() = default;

   uint16_t                 ff_;
   uint16_t                 ccbLength_;
   uint8_t                  mode_;
   uint8_t                  submode_;
   char                     precedence_;
   char                     classification_;
   std::string              messageOriginator_;
   uint8_t                  category_;
   uint8_t                  subcategory_;
   uint16_t                 userDefined_;
   uint8_t                  year_;
   uint8_t                  month_;
   uint8_t                  torDay_;
   uint8_t                  torHour_;
   uint8_t                  torMinute_;
   uint8_t                  numberOfDestinations_;
   std::vector<std::string> messageDestination_;
};

CcbHeader::CcbHeader() : p(std::make_unique<CcbHeaderImpl>()) {}
CcbHeader::~CcbHeader() = default;

CcbHeader::CcbHeader(CcbHeader&&) noexcept = default;
CcbHeader& CcbHeader::operator=(CcbHeader&&) noexcept = default;

uint16_t CcbHeader::ff() const
{
   return p->ff_;
}

uint16_t CcbHeader::ccb_length() const
{
   return p->ccbLength_;
}

uint8_t CcbHeader::mode() const
{
   return p->mode_;
}

uint8_t CcbHeader::submode() const
{
   return p->submode_;
}

char CcbHeader::precedence() const
{
   return p->precedence_;
}

char CcbHeader::classification() const
{
   return p->classification_;
}

std::string CcbHeader::message_originator() const
{
   return p->messageOriginator_;
}

uint8_t CcbHeader::category() const
{
   return p->category_;
}

uint8_t CcbHeader::subcategory() const
{
   return p->subcategory_;
}

uint16_t CcbHeader::user_defined() const
{
   return p->userDefined_;
}

uint8_t CcbHeader::year() const
{
   return p->year_;
}

uint8_t CcbHeader::month() const
{
   return p->month_;
}

uint8_t CcbHeader::tor_day() const
{
   return p->torDay_;
}

uint8_t CcbHeader::tor_hour() const
{
   return p->torHour_;
}

uint8_t CcbHeader::tor_minute() const
{
   return p->torMinute_;
}

uint8_t CcbHeader::number_of_destinations() const
{
   return p->numberOfDestinations_;
}

std::string CcbHeader::message_destination(uint8_t i) const
{
   return p->messageDestination_[i];
}

bool CcbHeader::Parse(std::istream& is)
{
   bool headerValid = true;

   is.read(reinterpret_cast<char*>(&p->ccbLength_), 2);

   p->ccbLength_ = ntohs(p->ccbLength_);

   p->ff_        = p->ccbLength_ >> 14;
   p->ccbLength_ = p->ccbLength_ & 0x3fff;

   p->messageOriginator_.resize(4);

   is.read(reinterpret_cast<char*>(&p->mode_), 1);
   is.read(reinterpret_cast<char*>(&p->submode_), 1);
   is.read(&p->precedence_, 1);
   is.read(&p->classification_, 1);
   is.read(p->messageOriginator_.data(), 4);
   is.read(reinterpret_cast<char*>(&p->category_), 1);
   is.read(reinterpret_cast<char*>(&p->subcategory_), 1);
   is.read(reinterpret_cast<char*>(&p->userDefined_), 2);
   is.read(reinterpret_cast<char*>(&p->year_), 1);
   is.read(reinterpret_cast<char*>(&p->month_), 1);
   is.read(reinterpret_cast<char*>(&p->torDay_), 1);
   is.read(reinterpret_cast<char*>(&p->torHour_), 1);
   is.read(reinterpret_cast<char*>(&p->torMinute_), 1);
   is.read(reinterpret_cast<char*>(&p->numberOfDestinations_), 1);

   p->userDefined_ = ntohs(p->userDefined_);

   p->messageDestination_.resize(p->numberOfDestinations_);

   for (uint8_t d = 0; d < p->numberOfDestinations_; d++)
   {
      p->messageDestination_[d].resize(4);
      is.read(p->messageDestination_[d].data(), 4);
   }

   if (is.eof())
   {
      logger_->debug("Reached end of file");
      headerValid = false;
   }

   return headerValid;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
