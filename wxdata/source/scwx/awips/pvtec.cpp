// Prevent redefinition of __cpp_lib_format
#if defined(_MSC_VER)
#   include <yvals_core.h>
#endif

// Enable chrono formatters
#ifndef __cpp_lib_format
#   define __cpp_lib_format 202110L
#endif

#include <scwx/awips/pvtec.hpp>
#include <scwx/util/logger.hpp>

#include <chrono>

#include <boost/assign.hpp>
#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>

#if !(defined(_MSC_VER) || defined(__clange__))
#   include <date/date.h>
#endif

namespace scwx
{
namespace awips
{

static const std::string logPrefix_ = "scwx::awips::pvtec";
static const auto        logger_    = util::Logger::Create(logPrefix_);

typedef boost::bimap<boost::bimaps::unordered_set_of<PVtec::ProductType>,
                     boost::bimaps::unordered_set_of<std::string>>
   ProductTypeCodesBimap;

static const ProductTypeCodesBimap productTypeCodes_ =
   boost::assign::list_of<ProductTypeCodesBimap::relation>    //
   (PVtec::ProductType::Operational, "O")                     //
   (PVtec::ProductType::Test, "T")                            //
   (PVtec::ProductType::Experimental, "E")                    //
   (PVtec::ProductType::OperationalWithExperimentalVtec, "X") //
   (PVtec::ProductType::Unknown, "??");

typedef boost::bimap<boost::bimaps::unordered_set_of<PVtec::Action>,
                     boost::bimaps::unordered_set_of<std::string>>
   ActionCodesBimap;

static const ActionCodesBimap actionCodes_ =
   boost::assign::list_of<ActionCodesBimap::relation> //
   (PVtec::Action::New, "NEW")                        //
   (PVtec::Action::Continued, "CON")                  //
   (PVtec::Action::ExtendedInArea, "EXA")             //
   (PVtec::Action::ExtendedInTime, "EXT")             //
   (PVtec::Action::ExtendedInAreaAndTime, "EXB")      //
   (PVtec::Action::Upgraded, "UPG")                   //
   (PVtec::Action::Canceled, "CAN")                   //
   (PVtec::Action::Expired, "EXP")                    //
   (PVtec::Action::Routine, "ROU")                    //
   (PVtec::Action::Correction, "COR")                 //
   (PVtec::Action::Unknown, "???");

class PVtecImpl
{
public:
   explicit PVtecImpl() :
       pVtecString_ {},
       fixedIdentifier_ {PVtec::ProductType::Unknown},
       action_ {PVtec::Action::Unknown},
       officeId_ {"????"},
       phenomenon_ {Phenomenon::Unknown},
       significance_ {Significance::Unknown},
       eventTrackingNumber_ {-1},
       eventBegin_ {},
       eventEnd_ {},
       valid_ {false}
   {
   }

   ~PVtecImpl() {}

   std::string pVtecString_;

   PVtec::ProductType fixedIdentifier_;
   PVtec::Action      action_;
   std::string        officeId_;
   Phenomenon         phenomenon_;
   Significance       significance_;
   int16_t            eventTrackingNumber_;

   std::chrono::system_clock::time_point eventBegin_;
   std::chrono::system_clock::time_point eventEnd_;

   bool valid_;
};

PVtec::PVtec() : p(std::make_unique<PVtecImpl>()) {}
PVtec::~PVtec() = default;

PVtec::PVtec(PVtec&&) noexcept            = default;
PVtec& PVtec::operator=(PVtec&&) noexcept = default;

PVtec::ProductType PVtec::fixed_identifier() const
{
   return p->fixedIdentifier_;
}

PVtec::Action PVtec::action() const
{
   return p->action_;
}

std::string PVtec::office_id() const
{
   return p->officeId_;
}

Phenomenon PVtec::phenomenon() const
{
   return p->phenomenon_;
}

Significance PVtec::significance() const
{
   return p->significance_;
}

int16_t PVtec::event_tracking_number() const
{
   return p->eventTrackingNumber_;
}

std::chrono::system_clock::time_point PVtec::event_begin() const
{
   return p->eventBegin_;
}

std::chrono::system_clock::time_point PVtec::event_end() const
{
   return p->eventEnd_;
}

bool PVtec::Parse(const std::string& s)
{
   using namespace std::chrono;

#if !(defined(_MSC_VER) || defined(__clang__))
   using namespace date;
#endif

   // P-VTEC takes the form:
   // /k.aaa.cccc.pp.s.####.yymmddThhnnZ-yymmddThhnnZ/
   // 012345678901234567890123456789012345678901234567
   static constexpr size_t pVtecLength_             = 48u;
   static constexpr size_t pVtecOffsetStart_        = 0u;
   static constexpr size_t pVtecOffsetIdentifier_   = 1u;
   static constexpr size_t pVtecOffsetAction_       = 3u;
   static constexpr size_t pVtecOffsetOfficeId_     = 7u;
   static constexpr size_t pVtecOffsetPhenomenon_   = 12u;
   static constexpr size_t pVtecOffsetSignificance_ = 15u;
   static constexpr size_t pVtecOffsetEventNumber_  = 17u;
   static constexpr size_t pVtecOffsetEventBegin_   = 22u;
   static constexpr size_t pVtecOffsetEventEnd_     = 35u;
   static constexpr size_t pVtecOffsetEnd_          = 47u;

   bool dataValid = (s.length() >= pVtecLength_ &&     //
                     s.at(pVtecOffsetStart_) == '/' && //
                     s.at(pVtecOffsetEnd_) == '/');

   if (dataValid)
   {
      p->pVtecString_ = s.substr(0, pVtecLength_);

      p->fixedIdentifier_ = GetProductType(s.substr(pVtecOffsetIdentifier_, 1));
      p->action_          = GetAction(s.substr(pVtecOffsetAction_, 3));
      p->officeId_        = s.substr(pVtecOffsetOfficeId_, 4);
      p->phenomenon_      = GetPhenomenon(s.substr(pVtecOffsetPhenomenon_, 2));
      p->significance_ = GetSignificance(s.substr(pVtecOffsetSignificance_, 1));

      std::string eventNumberString = s.substr(pVtecOffsetEventNumber_, 4);

      try
      {
         p->eventTrackingNumber_ =
            static_cast<int16_t>(std::stoi(eventNumberString));
      }
      catch (const std::exception& ex)
      {
         logger_->warn("Error parsing event tracking number: \"{}\" ({})",
                       eventNumberString,
                       ex.what());

         p->eventTrackingNumber_ = -1;
      }

      static const std::string dateTimeFormat {"%y%m%dT%H%MZ"};

      std::string sEventBegin = s.substr(pVtecOffsetEventBegin_, 12);
      std::string sEventEnd   = s.substr(pVtecOffsetEventEnd_, 12);

      std::istringstream ssEventBegin {sEventBegin};
      std::istringstream ssEventEnd {sEventEnd};

      std::chrono::sys_time<minutes> eventBegin;
      std::chrono::sys_time<minutes> eventEnd;

      ssEventBegin >> parse(dateTimeFormat, eventBegin);
      ssEventEnd >> parse(dateTimeFormat, eventEnd);

      if (!ssEventBegin.fail())
      {
         p->eventBegin_ = eventBegin;
      }
      else
      {
         // Time parsing expected to fail if time is "000000T0000Z"
         p->eventBegin_ = {};
      }

      if (!ssEventEnd.fail())
      {
         p->eventEnd_ = eventEnd;
      }
      else
      {
         // Time parsing expected to fail if time is "000000T0000Z"
         p->eventEnd_ = {};
      }
   }
   else
   {
      logger_->warn("Invalid P-VTEC: \"{}\"", s);
   }

   p->valid_ = dataValid;

   return dataValid;
}

PVtec::ProductType PVtec::GetProductType(const std::string& code)
{
   ProductType productType;

   if (productTypeCodes_.right.find(code) != productTypeCodes_.right.end())
   {
      productType = productTypeCodes_.right.at(code);
   }
   else
   {
      productType = ProductType::Unknown;

      logger_->debug("Unrecognized product code: \"{}\"", code);
   }

   return productType;
}

const std::string& PVtec::GetProductTypeCode(PVtec::ProductType productType)
{
   return productTypeCodes_.left.at(productType);
}

PVtec::Action PVtec::GetAction(const std::string& code)
{
   Action action;

   if (actionCodes_.right.find(code) != actionCodes_.right.end())
   {
      action = actionCodes_.right.at(code);
   }
   else
   {
      action = Action::Unknown;

      logger_->debug("Unrecognized action code: \"{}\"", code);
   }

   return action;
}

const std::string& PVtec::GetActionCode(PVtec::Action action)
{
   return actionCodes_.left.at(action);
}

} // namespace awips
} // namespace scwx
