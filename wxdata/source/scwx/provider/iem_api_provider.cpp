#include <scwx/provider/iem_api_provider.ipp>
#include <scwx/util/json.hpp>
#include <scwx/util/logger.hpp>

#include <boost/json.hpp>
#include <cpr/cpr.h>
#include <range/v3/iterator/operations.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/cartesian_product.hpp>
#include <range/v3/view/single.hpp>

#if (__cpp_lib_chrono < 201907L)
#   include <date/date.h>
#endif

namespace scwx::provider
{

static const std::string logPrefix_ = "scwx::provider::iem_api_provider";

const std::shared_ptr<spdlog::logger> IemApiProvider::logger_ =
   util::Logger::Create(logPrefix_);

const std::string IemApiProvider::kBaseUrl_ =
   "https://mesonet.agron.iastate.edu/api/1";

const std::string IemApiProvider::kListNwsTextProductsEndpoint_ =
   "/nws/afos/list.json";
const std::string IemApiProvider::kNwsTextProductEndpoint_ = "/nwstext/";

class IemApiProvider::Impl
{
public:
   explicit Impl()               = default;
   ~Impl()                       = default;
   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;
};

IemApiProvider::IemApiProvider() : p(std::make_unique<Impl>()) {}
IemApiProvider::~IemApiProvider() = default;

IemApiProvider::IemApiProvider(IemApiProvider&&) noexcept            = default;
IemApiProvider& IemApiProvider::operator=(IemApiProvider&&) noexcept = default;

boost::outcome_v2::result<std::vector<types::iem::AfosEntry>>
IemApiProvider::ListTextProducts(std::chrono::sys_days           date,
                                 std::optional<std::string_view> optionalCccc,
                                 std::optional<std::string_view> optionalPil)
{
   const std::string_view cccc =
      optionalCccc.has_value() ? optionalCccc.value() : std::string_view {};
   const std::string_view pil =
      optionalPil.has_value() ? optionalPil.value() : std::string_view {};

   const auto dateArray = std::array {date};
   const auto ccccArray = std::array {cccc};
   const auto pilArray  = std::array {pil};

   return ListTextProducts(dateArray, ccccArray, pilArray);
}

boost::outcome_v2::result<std::vector<types::iem::AfosEntry>>
IemApiProvider::ProcessTextProductLists(
   std::vector<cpr::AsyncResponse>& asyncResponses)
{
   std::vector<types::iem::AfosEntry> textProducts {};

   for (auto& asyncResponse : asyncResponses)
   {
      auto response = asyncResponse.get();

      const boost::json::value json = util::json::ReadJsonString(response.text);

      if (response.status_code == cpr::status::HTTP_OK)
      {
         // Get AFOS list from response
         auto converted = boost::json::try_value_to<types::iem::AfosList>(json);

         if (!converted.has_error())
         {
            types::iem::AfosList entries = std::move(*converted);

            textProducts.insert(textProducts.end(),
                                std::make_move_iterator(entries.data_.begin()),
                                std::make_move_iterator(entries.data_.end()));
         }
         else
         {
            logger_->warn("Error parsing JSON: {}",
                          converted.error().message());
            return boost::system::errc::make_error_code(
               boost::system::errc::bad_message);
         }
      }
      else if (response.status_code == cpr::status::HTTP_BAD_REQUEST &&
               json != nullptr)
      {
         // Log bad request details
         const auto badRequest =
            boost::json::try_value_to<types::iem::BadRequest>(json);

         if (!badRequest.has_error())
         {
            logger_->warn("ListTextProducts bad request: {}",
                          badRequest->detail_);
         }
         else
         {
            // Unexpected bad response
            logger_->warn("Error parsing bad response: {}",
                          badRequest.error().message());
         }

         return boost::system::errc::make_error_code(
            boost::system::errc::invalid_argument);
      }
      else if (response.status_code == cpr::status::HTTP_UNPROCESSABLE_ENTITY &&
               json != nullptr)
      {
         // Log validation error details
         const auto error =
            boost::json::try_value_to<types::iem::ValidationError>(json);

         if (!error.has_error())
         {
            logger_->warn("ListTextProducts validation error: {}",
                          error->detail_.at(0).msg_);
         }
         else
         {
            // Unexpected bad response
            logger_->warn("Error parsing validation error: {}",
                          error.error().message());
         }

         return boost::system::errc::make_error_code(
            boost::system::errc::no_message_available);
      }
      else
      {
         logger_->warn("Could not list text products: {}",
                       response.status_line);

         return boost::system::errc::make_error_code(
            boost::system::errc::no_message);
      }
   }

   logger_->debug("Found {} products", textProducts.size());

   return textProducts;
}

std::vector<std::shared_ptr<awips::TextProductFile>>
IemApiProvider::ProcessTextProductFiles(
   std::vector<std::pair<std::string, cpr::AsyncResponse>>& asyncResponses)
{
   std::vector<std::shared_ptr<awips::TextProductFile>> textProductFiles;

   for (auto& asyncResponse : asyncResponses)
   {
      auto response = asyncResponse.second.get();

      if (response.status_code == cpr::status::HTTP_OK)
      {
         // Load file
         auto& productId = asyncResponse.first;
         const std::shared_ptr<awips::TextProductFile> textProductFile {
            std::make_shared<awips::TextProductFile>()};
         std::istringstream responseBody {response.text};
         if (textProductFile->LoadData(productId, responseBody))
         {
            textProductFiles.push_back(textProductFile);
         }
      }
      else
      {
         logger_->warn("Could not load text product: {} ({})",
                       asyncResponse.first,
                       response.status_line);
      }
   }

   logger_->debug("Loaded {} text products", textProductFiles.size());

   return textProductFiles;
}

} // namespace scwx::provider
