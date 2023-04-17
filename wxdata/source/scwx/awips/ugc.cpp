#include <scwx/awips/ugc.hpp>
#include <scwx/util/logger.hpp>

#include <map>
#include <regex>

#include <boost/assign.hpp>
#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <boost/tokenizer.hpp>

namespace scwx
{
namespace awips
{

static const std::string logPrefix_ = "scwx::awips::ugc";
static const auto        logger_    = util::Logger::Create(logPrefix_);

enum class UgcFormat
{
   Counties,
   Zones,
   Unknown
};

typedef boost::bimap<boost::bimaps::unordered_set_of<UgcFormat>,
                     boost::bimaps::unordered_set_of<char>>
   UgcFormatBimap;

static const UgcFormatBimap ugcFormatMap_ =
   boost::assign::list_of<UgcFormatBimap::relation> //
   (UgcFormat::Counties, 'C')                       //
   (UgcFormat::Zones, 'Z')                          //
   (UgcFormat::Unknown, '?');

class UgcImpl
{
public:
   explicit UgcImpl() :
       ugcString_ {},
       format_ {UgcFormat::Unknown},
       fipsIdMap_ {},
       productExpiration_ {},
       valid_ {false}
   {
   }

   ~UgcImpl() {}

   std::vector<std::string> ugcString_;

   UgcFormat                                  format_;
   std::map<std::string, std::list<uint16_t>> fipsIdMap_;
   std::string                                productExpiration_;

   bool valid_;
};

Ugc::Ugc() : p(std::make_unique<UgcImpl>()) {}
Ugc::~Ugc() = default;

Ugc::Ugc(Ugc&&) noexcept            = default;
Ugc& Ugc::operator=(Ugc&&) noexcept = default;

std::vector<std::string> Ugc::states() const
{
   std::vector<std::string> states {};
   states.reserve(p->fipsIdMap_.size());

   for (auto& entry : p->fipsIdMap_)
   {
      states.push_back(entry.first);
   }

   return states;
}

std::vector<std::string> Ugc::fips_ids() const
{
   std::vector<std::string> fipsIds {};

   for (auto& fipsIdList : p->fipsIdMap_)
   {
      for (auto& id : fipsIdList.second)
      {
         fipsIds.push_back(fmt::format("{}{}{:03}",
                                       fipsIdList.first,
                                       ugcFormatMap_.left.at(p->format_),
                                       id));
      }
   }

   return fipsIds;
}

std::string Ugc::product_expiration() const
{
   return p->productExpiration_;
}

bool Ugc::Parse(const std::vector<std::string>& ugcString)
{
   bool dataValid = false;

   // UGC takes the form SSFNNN-NNN>NNN-SSFNNN-DDHHMM- (NWSI 10-1702)
   static const std::regex reStart {"[A-Z]{2}[CZ]([0-9]{3}|ALL)"};
   static const std::regex reAnyFipsId {"([0-9]{3}|ALL)"};
   static const std::regex reSpecificFipsId {"(?!0{3})[0-9]{3}"};
   static const std::regex reProductExpiration {"[0-9]{6}"};

   std::stringstream ugcStream;
   for (auto& line : ugcString)
   {
      ugcStream << line;
   }

   // Concatenate UGC lines into a single string
   std::string ugc {};
   for (const std::string& line : ugcString)
   {
      ugc += line;
   }

   boost::char_separator<char> sectionDelimiter("-");
   boost::char_separator<char> rangeDelimiter(">");
   boost::tokenizer            tokens(ugc, sectionDelimiter);

   std::string currentState {};

   for (auto& token : tokens)
   {
      // Product Expiration is the final token
      if (std::regex_match(token, reProductExpiration))
      {
         p->productExpiration_ = token;
         dataValid             = true;
         break;
      }

      // Tokenize string again by ">" (note there will always be at least one
      // range token)
      boost::tokenizer rangeTokens(token, rangeDelimiter);
      const size_t     numRangeTokens =
         std::distance(rangeTokens.begin(), rangeTokens.end());
      bool        tokenValid = true;
      bool        allFipsIds = false;
      auto        tokenIt    = rangeTokens.begin();
      std::string firstToken {tokenIt.current_token()};
      UgcFormat   currentFormat {p->format_};
      std::string firstFipsId {};
      std::string secondFipsId {};

      // Look for the start of the UGC string (may be multiple per UGC string
      // for multiple states, territories, or marine area)
      if (std::regex_match(firstToken, reStart))
      {
         currentState  = firstToken.substr(0, 2);
         currentFormat = ugcFormatMap_.right.at(firstToken.at(2));
         firstFipsId   = firstToken.substr(3, 3);

         // The UGC string must contain counties or zones, but not both
         if (p->format_ != UgcFormat::Unknown && p->format_ != currentFormat)
         {
            tokenValid = false;
         }
      }
      // Look for additional FIPS IDs in the UGC string
      else if (!currentState.empty() &&
               std::regex_match(firstToken, reAnyFipsId))
      {
         firstFipsId = firstToken;
      }
      // If we see anything else, the UGC token is invalid
      else
      {
         tokenValid = false;
      }

      // All counties or zones are specified by using "000" or "ALL"
      if (firstFipsId == "000" || firstFipsId == "ALL")
      {
         allFipsIds = true;
      }

      // Parse the second token in a range (i.e., NNN>XXX)
      if (numRangeTokens == 2)
      {
         std::string secondToken {(++tokenIt).current_token()};

         if (std::regex_match(secondToken, reSpecificFipsId))
         {
            secondFipsId = secondToken;
         }
         else
         {
            tokenValid = false;
         }
      }

      // Check validity before using parsed data
      if (!tokenValid || numRangeTokens > 2 ||
          (allFipsIds && numRangeTokens > 1))
      {
         logger_->warn("Invalid token: {}", token);
         break;
      }

      p->format_    = currentFormat;
      auto& fipsIds = p->fipsIdMap_[currentState];

      if (allFipsIds)
      {
         fipsIds.push_back(0);
      }
      else
      {
         // Insert the FIPS ID (NNN) from the token
         fipsIds.push_back(static_cast<uint16_t>(std::stoul(firstFipsId)));

         if (numRangeTokens == 2)
         {
            // Insert the remainder of the FIPS IDs in the range given by the
            // token (NNN>XXX)
            const uint16_t first = fipsIds.back();
            const uint16_t last =
               static_cast<uint16_t>(std::stoul(secondFipsId));

            for (uint16_t i = first + 1; i <= last; i++)
            {
               fipsIds.push_back(i);
            }
         }
      }
   }

   p->valid_ = dataValid;

   if (!dataValid)
   {
      p->fipsIdMap_.clear();
   }

   return dataValid;
}

} // namespace awips
} // namespace scwx
