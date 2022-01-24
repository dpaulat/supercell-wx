#include <scwx/awips/wmo_header.hpp>
#include <scwx/util/streams.hpp>

#include <istream>
#include <sstream>
#include <string>

#include <boost/log/trivial.hpp>

#ifdef WIN32
#   include <WinSock2.h>
#else
#   include <arpa/inet.h>
#endif

namespace scwx
{
namespace awips
{

static const std::string logPrefix_ = "[scwx::awips::wmo_header] ";

class WmoHeaderImpl
{
public:
   explicit WmoHeaderImpl() :
       sequenceNumber_ {},
       dataType_ {},
       geographicDesignator_ {},
       bulletinId_ {},
       icao_ {},
       dateTime_ {},
       bbbIndicator_ {},
       productCategory_ {},
       productDesignator_ {}
   {
   }
   ~WmoHeaderImpl() = default;

   bool operator==(const WmoHeaderImpl& o) const;

   std::string sequenceNumber_;
   std::string dataType_;
   std::string geographicDesignator_;
   std::string bulletinId_;
   std::string icao_;
   std::string dateTime_;
   std::string bbbIndicator_;
   std::string productCategory_;
   std::string productDesignator_;
};

WmoHeader::WmoHeader() : p(std::make_unique<WmoHeaderImpl>()) {}
WmoHeader::~WmoHeader() = default;

WmoHeader::WmoHeader(WmoHeader&&) noexcept = default;
WmoHeader& WmoHeader::operator=(WmoHeader&&) noexcept = default;

bool WmoHeader::operator==(const WmoHeader& o) const
{
   return (*p.get() == *o.p.get());
}

bool WmoHeaderImpl::operator==(const WmoHeaderImpl& o) const
{
   return (sequenceNumber_ == o.sequenceNumber_ &&             //
           dataType_ == o.dataType_ &&                         //
           geographicDesignator_ == o.geographicDesignator_ && //
           bulletinId_ == o.bulletinId_ &&                     //
           icao_ == o.icao_ &&                                 //
           dateTime_ == o.dateTime_ &&                         //
           bbbIndicator_ == o.bbbIndicator_ &&                 //
           productCategory_ == o.productCategory_ &&           //
           productDesignator_ == o.productDesignator_);
}

std::string WmoHeader::sequence_number() const
{
   return p->sequenceNumber_;
}

std::string WmoHeader::data_type() const
{
   return p->dataType_;
}

std::string WmoHeader::geographic_designator() const
{
   return p->geographicDesignator_;
}

std::string WmoHeader::bulletin_id() const
{
   return p->bulletinId_;
}

std::string WmoHeader::icao() const
{
   return p->icao_;
}

std::string WmoHeader::date_time() const
{
   return p->dateTime_;
}

std::string WmoHeader::bbb_indicator() const
{
   return p->bbbIndicator_;
}

std::string WmoHeader::product_category() const
{
   return p->productCategory_;
}

std::string WmoHeader::product_designator() const
{
   return p->productDesignator_;
}

bool WmoHeader::Parse(std::istream& is)
{
   bool headerValid = true;

   std::string sohLine;
   std::string sequenceLine;
   std::string wmoLine;
   std::string awipsLine;

   if (is.peek() == 0x01)
   {
      util::getline(is, sohLine);
      util::getline(is, sequenceLine);
   }

   util::getline(is, wmoLine);
   util::getline(is, awipsLine);

   if (is.eof())
   {
      BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Reached end of file";
      headerValid = false;
   }
   else
   {
      // Remove delimiters from the end of the line
      while (sequenceLine.ends_with(' '))
      {
         sequenceLine.erase(sequenceLine.length() - 1);
      }
   }

   // Transmission Header:
   // [SOH]
   // nnn

   if (headerValid && !sequenceLine.empty())
   {
      p->sequenceNumber_ = sequenceLine;
   }

   // WMO Abbreviated Heading Line:
   // T1T2A1A2ii CCCC YYGGgg (BBB)

   if (headerValid)
   {
      std::string              token;
      std::istringstream       wmoTokens(wmoLine);
      std::vector<std::string> wmoTokenList;

      while (wmoTokens >> token)
      {
         wmoTokenList.push_back(std::move(token));
      }

      if (wmoTokenList.size() < 3 || wmoTokenList.size() > 4)
      {
         BOOST_LOG_TRIVIAL(debug)
            << logPrefix_ << "Invalid number of WMO tokens";
         headerValid = false;
      }
      else if (wmoTokenList[0].size() != 6)
      {
         BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "WMO identifier malformed";
         headerValid = false;
      }
      else if (wmoTokenList[1].size() != 4)
      {
         BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "ICAO malformed";
         headerValid = false;
      }
      else if (wmoTokenList[2].size() != 6)
      {
         BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Date/time malformed";
         headerValid = false;
      }
      else if (wmoTokenList.size() == 4 && wmoTokenList[3].size() != 3)
      {
         // BBB indicator is optional
         BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "BBB indicator malformed";
         headerValid = false;
      }
      else
      {
         p->dataType_             = wmoTokenList[0].substr(0, 2);
         p->geographicDesignator_ = wmoTokenList[0].substr(2, 2);
         p->bulletinId_           = wmoTokenList[0].substr(4, 2);
         p->icao_                 = wmoTokenList[1];
         p->dateTime_             = wmoTokenList[2];

         if (wmoTokenList.size() == 4)
         {
            p->bbbIndicator_ = wmoTokenList[3];
         }
         else
         {
            p->bbbIndicator_ = "";
         }
      }
   }

   // AWIPS Identifer Line:
   // NNNxxx

   if (headerValid)
   {
      if (awipsLine.size() != 6)
      {
         BOOST_LOG_TRIVIAL(debug)
            << logPrefix_ << "AWIPS Identifier Line bad size";
         headerValid = false;
      }
      else
      {
         p->productCategory_   = awipsLine.substr(0, 3);
         p->productDesignator_ = awipsLine.substr(3, 3);
      }
   }

   return headerValid;
}

} // namespace awips
} // namespace scwx
