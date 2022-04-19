#include <scwx/awips/significance.hpp>
#include <scwx/util/logger.hpp>

#include <boost/assign.hpp>
#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>

namespace scwx
{
namespace awips
{

static const std::string logPrefix_ = "scwx::awips::significance";
static const auto        logger_    = util::Logger::Create(logPrefix_);

typedef boost::bimap<boost::bimaps::unordered_set_of<Significance>,
                     boost::bimaps::unordered_set_of<std::string>>
   SignificanceCodesBimap;

static const SignificanceCodesBimap significanceCodes_ =
   boost::assign::list_of<SignificanceCodesBimap::relation> //
   (Significance::Warning, "W")                             //
   (Significance::Watch, "A")                               //
   (Significance::Advisory, "Y")                            //
   (Significance::Statement, "S")                           //
   (Significance::Forecast, "F")                            //
   (Significance::Outlook, "O")                             //
   (Significance::Synopsis, "N")                            //
   (Significance::Unknown, "?");

static const std::unordered_map<Significance, std::string> significanceText_ {
   {Significance::Warning, "Warning"},     //
   {Significance::Watch, "Watch"},         //
   {Significance::Advisory, "Advisory"},   //
   {Significance::Statement, "Statement"}, //
   {Significance::Forecast, "Forecast"},   //
   {Significance::Outlook, "Outlook"},     //
   {Significance::Synopsis, "Synopsis"},   //
   {Significance::Unknown, "Unknown"}};

Significance GetSignificance(const std::string& code)
{
   Significance significance;

   if (significanceCodes_.right.find(code) != significanceCodes_.right.end())
   {
      significance = significanceCodes_.right.at(code);
   }
   else
   {
      significance = Significance::Unknown;

      logger_->debug("Unrecognized code: \"{}\"", code);
   }

   return significance;
}

const std::string& GetSignificanceCode(Significance significance)
{
   return significanceCodes_.left.at(significance);
}

const std::string& GetSignificanceText(Significance significance)
{
   return significanceText_.at(significance);
}

} // namespace awips
} // namespace scwx
