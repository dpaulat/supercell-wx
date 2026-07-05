#include <scwx/config/ondas_config.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/streams.hpp>
#include <scwx/util/strings.hpp>
#include <scwx/util/time.hpp>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <re2/re2.h>

namespace scwx::config
{

static const std::string logPrefix_ = "scwx::config::ondas_config";
static const auto        logger_    = util::Logger::Create(logPrefix_);

class OndasConfig::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;

   Impl(const Impl&)            = delete;
   Impl& operator=(const Impl&) = delete;
   Impl(Impl&&)                 = delete;
   Impl& operator=(Impl&&)      = delete;

   void ProcessLine(const std::string& line);

   std::string                                         listFile_ {"dir.list"};
   std::vector<std::string>                            sites_ {};
   boost::unordered_flat_map<std::string, std::string> directoryTemplates_ {};
};

OndasConfig::OndasConfig() : p(std::make_unique<Impl>()) {}
OndasConfig::~OndasConfig() = default;

OndasConfig::OndasConfig(OndasConfig&&) noexcept            = default;
OndasConfig& OndasConfig::operator=(OndasConfig&&) noexcept = default;

std::string OndasConfig::list_file() const
{
   return p->listFile_;
}

std::vector<std::string> OndasConfig::sites() const
{
   return p->sites_;
}

std::vector<std::string> OndasConfig::products() const
{
   std::vector<std::string> products;
   for (const auto& [key, value] : p->directoryTemplates_)
   {
      products.emplace_back(key);
   }
   return products;
}

void OndasConfig::Parse(std::istream& is)
{
   logger_->debug("Parse()");

   std::string line {};

   while (util::getline(is, line))
   {
      // Remove extra spacing from line
      boost::trim(line);

      if (line.size() > 1)
      {
         p->ProcessLine(line);
      }
   }
}

void OndasConfig::Impl::ProcessLine(const std::string& line)
{
   static const std::string listFileKey_ {"ListFile:"};
   static const std::string siteKey_ {"Site:"};
   static const std::string productKey_ {"Product:"};

   if (boost::istarts_with(line, listFileKey_))
   {
      // ListFile: filename
      listFile_ = line.substr(listFileKey_.size());
      boost::trim(listFile_);
   }
   else if (boost::istarts_with(line, siteKey_))
   {
      // Site: radar_site
      std::string site = line.substr(siteKey_.size());
      boost::trim(site);
      sites_.emplace_back(std::move(site));
   }
   else if (boost::istarts_with(line, productKey_))
   {
      // ProductMapping: product_code directory_template
      std::vector<std::string> tokenList =
         util::ParseTokens(line, {" "}, productKey_.size());

      if (tokenList.size() >= 2)
      {
         std::string productCode       = std::move(tokenList[0]);
         std::string directoryTemplate = std::move(tokenList[1]);
         boost::trim(productCode);
         boost::trim(directoryTemplate);
         directoryTemplates_.emplace(std::move(productCode),
                                     std::move(directoryTemplate));
      }
      else
      {
         logger_->warn("ProductMapping statement malformed: {}", line);
      }
   }
}

std::string OndasConfig::ApplySiteSubstitution(const std::string& radarSite,
                                               const std::string& product) const
{
   logger_->trace(
      "ApplySiteSubstitution(): radarSite={}, product={}", radarSite, product);

   std::string result {};

   if (radarSite.size() < 4)
   {
      logger_->warn("Radar site ID must be at least 4 characters: {}",
                    radarSite);
      return result;
   }

   const auto it = p->directoryTemplates_.find(product);
   if (it != p->directoryTemplates_.end())
   {
      const std::string& templateStr = it->second;

      result = templateStr;

      // Replace SSSS/SSS/ssss/sss patterns
      const std::string upperFour  = boost::to_upper_copy(radarSite);
      const std::string lowerFour  = boost::to_lower_copy(radarSite);
      const std::string upperThree = upperFour.substr(1); // "ILN"
      const std::string lowerThree = lowerFour.substr(1); // "iln"

      // Replace patterns (order matters: check 4-char first)
      boost::replace_all(result, "SSSS", upperFour);
      boost::replace_all(result, "ssss", lowerFour);
      boost::replace_all(result, "SSS", upperThree);
      boost::replace_all(result, "sss", lowerThree);
   }
   else
   {
      logger_->warn("No template found for product: {}", product);
   }

   return result;
}

std::chrono::system_clock::time_point
OndasConfig::GetTimePointFromFilename(const std::string& filename)
{
   // Filename/Timestamp Format (per ONDAS spec):
   // - Format: YYYYMMDD_HHMM (minimum) or SSSS_YYYYMMDD_HHMM
   // - Example: 20260131_1830 or KILN_20260131_1830
   // - Note: Some servers may include seconds or version suffix

   // The first 8 contiguous digits are used for the data (yyyymmdd), an
   // underscore, the next 4 contiguous digits are the time (hhmm) in 24
   // hour notation. Date and time MUST be in GMT/UTC timezone.

   // June 26, 2005 @ 11:45PM UTC would be 20050626_2145

   static constexpr re2::LazyRE2 re {R"((\d{8}_\d{4}))"};

   std::chrono::system_clock::time_point time {};
   std::string                           dateTimeStr {};

   if (!RE2::PartialMatch(filename, *re, &dateTimeStr))
   {
      logger_->warn("Invalid ONDAS timestamp format in key: \"{}\"", filename);
      return time;
   }

   // Match now contains the entire "YYYYMMDD_HHMM" substring
   // Parse using std::chrono::parse
   static const std::string timeFormat {"%Y%m%d_%H%M"};
   std::istringstream       ss(dateTimeStr);

   ss >> util::time::parse(timeFormat, time);

   if (ss.fail())
   {
      logger_->warn("Failed to parse ONDAS timestamp: \"{}\"", dateTimeStr);
   }

   return time;
}

} // namespace scwx::config
