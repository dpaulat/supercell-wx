#define LIBXML_HTML_ENABLED

#include <scwx/network/dir_list.hpp>
#include <scwx/util/logger.hpp>

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#include <boost/algorithm/string/trim.hpp>
#include <cpr/cpr.h>
#include <libxml/HTMLparser.h>

#if (__cpp_lib_chrono < 201907L)
#   include <date/date.h>
#endif

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

namespace scwx
{
namespace network
{

static const std::string logPrefix_ = "scwx::network::dir_list";
static const auto        logger_    = util::Logger::Create(logPrefix_);

static const cpr::SslOptions  kSslOptions_ = cpr::Ssl(cpr::ssl::TLSv1_2 {});
static const cpr::HttpVersion kHttpVersion_ {
   cpr::HttpVersionCode::VERSION_2_0_TLS};

class DirListSAXHandler
{
public:
   DirListSAXHandler() = delete;
   static void StartDocument(void* userData);
   static void EndDocument(void* userData);
   static void
   StartElement(void* userData, const xmlChar* name, const xmlChar** attrs);
   static void EndElement(void* userData, const xmlChar* name);
   static void Characters(void* userData, const xmlChar* ch, int len);
   static void Warning(void* userData, const char* msg, ...);
   static void Error(void* userData, const char* msg, ...);
   static void Critical(void* userData, const char* msg, ...);
};

struct DirListSAXData
{
   enum class State
   {
      FindingLink,
      FoundLink,
      UpdateLinkTimestamp,
      UpdateLinkSize
   };
   State  state_ {State::FindingLink};
   size_t warningCount_ {0u};
   size_t errorCount_ {0u};
   size_t criticalCount_ {0u};

   std::vector<DirListRecord> records_;
};

// Unspecified fields are initialized to zero, ignore warning
#if defined(__GNUC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

static htmlSAXHandler saxHandler_ //
   {.startElement = &DirListSAXHandler::StartElement,
    .endElement   = &DirListSAXHandler::EndElement,
    .characters   = &DirListSAXHandler::Characters,
    .warning      = &DirListSAXHandler::Warning,
    .error        = &DirListSAXHandler::Error,
    .fatalError   = &DirListSAXHandler::Critical};

#if defined(__GNUC__)
#   pragma GCC diagnostic pop
#endif

std::vector<DirListRecord> DirList(const std::string& baseUrl)
{
   using namespace std::chrono;

   logger_->trace("DirList: {}", baseUrl);

   cpr::Response response =
      cpr::Get(cpr::Url {baseUrl}, kSslOptions_, kHttpVersion_);
   DirListSAXData saxData {};

   if (response.status_code != cpr::status::HTTP_OK)
   {
      logger_->warn("Bad response from {}: {} ({})",
                    baseUrl,
                    response.error.message,
                    response.status_code);
   }
   else
   {
      htmlParserCtxtPtr ctxt = htmlNewSAXParserCtxt(&saxHandler_, &saxData);
      htmlDocPtr        doc  = nullptr;

      if (ctxt != nullptr)
      {
         doc = htmlCtxtReadDoc(
            ctxt,
            reinterpret_cast<const xmlChar*>(response.text.c_str()),
            baseUrl.c_str(),
            nullptr,
            HTML_PARSE_NONET);
         htmlFreeParserCtxt(ctxt);
      }

      if (doc != nullptr)
      {
         xmlFreeDoc(doc);
      }
   }

   return saxData.records_;
}

void DirListSAXHandler::StartElement(void*           userData,
                                     const xmlChar*  name,
                                     const xmlChar** attrs)
{
   logger_->trace("SAX: Start Element: {}",
                  reinterpret_cast<const char*>(name));

   DirListSAXData* data = reinterpret_cast<DirListSAXData*>(userData);

   if (strcmp(reinterpret_cast<const char*>(name), "a") == 0)
   {
      // If an "a" element is found, search for an "href" attribute
      for (int i = 0; attrs != nullptr && attrs[i] != nullptr; ++i)
      {
         if (i > 0 &&
             strcmp(reinterpret_cast<const char*>(attrs[i - 1]), "href") == 0)
         {
            // If the "href" attribute is found, treat this as a new file
            std::string filename {reinterpret_cast<const char*>(attrs[i])};
            std::filesystem::file_type fileType;

            // Determine if the file is a directory
            if (filename.ends_with("/"))
            {
               filename.pop_back();
               fileType = std::filesystem::file_type::directory;
            }
            else
            {
               fileType = std::filesystem::file_type::regular;
            }

            // If the filename is valid, add it as a record
            if (filename.size() > 0 && !filename.starts_with("?") &&
                // And the filename is not a duplicate of the previous record
                (data->records_.size() == 0 ||
                 data->records_.back().filename_ != filename))
            {
               data->records_.emplace_back(filename, fileType);
               data->state_ = DirListSAXData::State::FoundLink;
               break;
            }
         }
      }
   }
   for (int i = 0; attrs != nullptr && attrs[i] != nullptr; ++i)
   {
      logger_->trace("     Attribute: {}",
                     reinterpret_cast<const char*>(attrs[i]));
   }
}

void DirListSAXHandler::EndElement(void* userData, const xmlChar* name)
{
   logger_->trace("SAX: End Element: {}", reinterpret_cast<const char*>(name));

   DirListSAXData* data = reinterpret_cast<DirListSAXData*>(userData);

   if (data->state_ == DirListSAXData::State::FoundLink &&
       strcmp(reinterpret_cast<const char*>(name), "a") == 0)
   {
      // The "a" element is closed, so begin looking for the timestamp
      data->state_ = DirListSAXData::State::UpdateLinkTimestamp;
   }
}

void DirListSAXHandler::Characters(void* userData, const xmlChar* ch, int len)
{
   std::string characters(reinterpret_cast<const char*>(ch), len);
   logger_->trace("SAX: Characters: {}", characters);

   DirListSAXData* data = reinterpret_cast<DirListSAXData*>(userData);

   if (data->state_ == DirListSAXData::State::UpdateLinkTimestamp)
   {
      using namespace std::chrono;

#if (__cpp_lib_chrono < 201907L)
      using namespace date;
#endif

      // Date time format: yyyy-mm-dd hh:mm
      static const std::string kDateTimeFormat {"%Y-%m-%d %H:%M"};

      // Attempt to parse the date time
      std::istringstream             ssCharacters {characters};
      std::chrono::sys_time<minutes> mtime;
      ssCharacters >> parse(kDateTimeFormat, mtime);

      if (!ssCharacters.fail())
      {
         // Date time parsing succeeded, look for link size
         auto& record  = data->records_.back();
         record.mtime_ = mtime;

         if (record.type_ == std::filesystem::file_type::directory)
         {
            // If the record is a directory, there is no size, skip to next link
            data->state_ = DirListSAXData::State::FindingLink;
         }
         else
         {
            // After the time is parsed, get the file size
            data->state_ = DirListSAXData::State::UpdateLinkSize;
         }
      }
   }
   else if (data->state_ == DirListSAXData::State::UpdateLinkSize)
   {
      // Trim the file size string
      std::string fileSizeString {characters};
      boost::trim(fileSizeString);

      size_t fileSize   = 0u;
      size_t multiplier = 1u;

      // Look for size suffix
      if (fileSizeString.ends_with("K"))
      {
         fileSizeString.pop_back();
         multiplier = 1024u;
      }
      else if (fileSizeString.ends_with("M"))
      {
         fileSizeString.pop_back();
         multiplier = 1024u * 1024u;
      }
      else if (fileSizeString.ends_with("G"))
      {
         fileSizeString.pop_back();
         multiplier = 1024u * 1024u * 1024u;
      }
      else if (fileSizeString.ends_with("T"))
      {
         fileSizeString.pop_back();
         multiplier = 1024ull * 1024ull * 1024ull * 1024ull;
      }

      try
      {
         // Parse the remaining file size string, and multiply by the suffix
         fileSize = static_cast<size_t>(std::stod(fileSizeString) * multiplier);
         data->records_.back().size_ = fileSize;

         // Look for the next link
         data->state_ = DirListSAXData::State::FindingLink;
      }
      catch (const std::exception&)
      {
         // This was something other than a file size
      }
   }
}

void DirListSAXHandler::Warning(void* /* userData */, const char* msg, ...)
{
   logger_->warn("SAX: {}", msg);
}

void DirListSAXHandler::Error(void* /* userData */, const char* msg, ...)
{
   logger_->error("SAX: {}", msg);
}

void DirListSAXHandler::Critical(void* /* userData */, const char* msg, ...)
{
   logger_->critical("SAX: {}", msg);
}

} // namespace network
} // namespace scwx
