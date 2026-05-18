#define LIBXML_HTML_ENABLED

#include <scwx/spc/spc_md_provider.hpp>
#include <scwx/network/cpr.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/zip/zip_stream_reader.hpp>

#include <cpr/cpr.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/HTMLparser.h>

#include <fmt/format.h>

#include <regex>
#include <sstream>
#include <string>

namespace scwx::spc
{

static const std::string logPrefix_ = "scwx::spc::spc_md_provider";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static const std::string kMdUrl_ =
   "https://www.spc.noaa.gov/products/md/ActiveMD.kmz";

/**
 * Recursively searches an XML subtree for <Placemark> elements
 * containing <Polygon> -> <outerBoundaryIs> -> <LinearRing> ->
 * <coordinates>.  Parsed rings are appended to md.rings_.
 */
static void ParsePlacemarksRecursive(xmlNodePtr node, MesoscaleDiscussion& md)
{
   if (node == nullptr)
   {
      return;
   }

   if (node->type == XML_ELEMENT_NODE &&
       xmlStrcmp(node->name, reinterpret_cast<const xmlChar*>("Placemark")) ==
          0)
   {
      // Look for Polygon -> outerBoundaryIs -> LinearRing -> coordinates
      for (xmlNodePtr pmChild = node->children; pmChild != nullptr;
           pmChild            = pmChild->next)
      {
         if (pmChild->type != XML_ELEMENT_NODE)
         {
            continue;
         }

         if (xmlStrcmp(pmChild->name,
                       reinterpret_cast<const xmlChar*>("Polygon")) == 0)
         {
            // Navigate to coordinates
            xmlNodePtr outerBoundary = nullptr;
            for (xmlNodePtr polyChild = pmChild->children; polyChild != nullptr;
                 polyChild            = polyChild->next)
            {
               if (polyChild->type == XML_ELEMENT_NODE &&
                   xmlStrcmp(
                      polyChild->name,
                      reinterpret_cast<const xmlChar*>("outerBoundaryIs")) == 0)
               {
                  outerBoundary = polyChild;
                  break;
               }
            }
            if (outerBoundary == nullptr)
            {
               continue;
            }

            xmlNodePtr linearRing = nullptr;
            for (xmlNodePtr obChild = outerBoundary->children;
                 obChild != nullptr;
                 obChild = obChild->next)
            {
               if (obChild->type == XML_ELEMENT_NODE &&
                   xmlStrcmp(obChild->name,
                             reinterpret_cast<const xmlChar*>("LinearRing")) ==
                      0)
               {
                  linearRing = obChild;
                  break;
               }
            }
            if (linearRing == nullptr)
            {
               continue;
            }

            xmlNodePtr coordsNode = nullptr;
            for (xmlNodePtr lrChild = linearRing->children; lrChild != nullptr;
                 lrChild            = lrChild->next)
            {
               if (lrChild->type == XML_ELEMENT_NODE &&
                   xmlStrcmp(lrChild->name,
                             reinterpret_cast<const xmlChar*>("coordinates")) ==
                      0)
               {
                  coordsNode = lrChild;
                  break;
               }
            }
            if (coordsNode == nullptr)
            {
               continue;
            }

            xmlChar* coordsText = xmlNodeGetContent(coordsNode);
            if (coordsText == nullptr)
            {
               continue;
            }

            std::string coordStr = reinterpret_cast<const char*>(coordsText);
            xmlFree(coordsText);

            // Parse "lon,lat,0 lon,lat,0 ..."
            std::vector<std::pair<double, double>> ring;
            std::istringstream                     coordStream(coordStr);
            std::string                            triple;

            while (coordStream >> triple)
            {
               try
               {
                  auto comma1 = triple.find(',');
                  if (comma1 == std::string::npos)
                  {
                     continue;
                  }
                  auto comma2 = triple.find(',', comma1 + 1);
                  if (comma2 == std::string::npos)
                  {
                     comma2 = triple.size();
                  }

                  double lon = std::stod(triple.substr(0, comma1));
                  double lat =
                     std::stod(triple.substr(comma1 + 1, comma2 - comma1 - 1));

                  ring.emplace_back(lat, lon);
               }
               catch (const std::exception&)
               {
                  logger_->warn("Skipping malformed coordinate triple: {}",
                                triple);
                  continue;
               }
            }

            if (!ring.empty())
            {
               md.rings_.push_back(std::move(ring));
            }
         }
      }

      return; // Found a Placemark at this level, done
   }

   // Recurse into children
   for (xmlNodePtr child = node->children; child != nullptr;
        child            = child->next)
   {
      ParsePlacemarksRecursive(child, md);
   }
}

class SpcMdProvider::Impl
{
public:
   Impl()  = default;
   ~Impl() = default;
};

SpcMdProvider::SpcMdProvider() : p(std::make_unique<Impl>()) {}
SpcMdProvider::~SpcMdProvider()                                   = default;
SpcMdProvider::SpcMdProvider(SpcMdProvider&&) noexcept            = default;
SpcMdProvider& SpcMdProvider::operator=(SpcMdProvider&&) noexcept = default;

const std::string& SpcMdProvider::kMdUrl()
{
   return kMdUrl_;
}

boost::outcome_v2::result<MdData> SpcMdProvider::FetchActiveMDs()
{
   logger_->info("Fetching Active MD KMZ: {}", kMdUrl_);

   ::cpr::Response r = ::cpr::Get(::cpr::Url {kMdUrl_},
                                  network::cpr::GetHeader(),
                                  network::cpr::GetDefaultTimeout(),
                                  network::cpr::GetDefaultConnectTimeout(),
                                  network::cpr::GetDefaultLowSpeed());

   if (r.status_code != 200)
   {
      logger_->warn(
         "FetchActiveMDs failed: HTTP {} for URL {}", r.status_code, kMdUrl_);
      return boost::outcome_v2::failure(
         std::make_error_code(std::errc::io_error));
   }

   logger_->info("FetchActiveMDs succeeded ({} bytes)", r.text.size());

   // Wrap raw response text (binary) into a stringstream for ZipStreamReader
   std::stringstream stream;
   stream.write(r.text.data(), static_cast<std::streamsize>(r.text.size()));

   scwx::zip::ZipStreamReader reader(stream);
   if (!reader.IsOpen())
   {
      logger_->warn("Failed to open KMZ as ZIP archive");
      return boost::outcome_v2::failure(
         std::make_error_code(std::errc::io_error));
   }

   std::string kmlContent;
   if (!reader.ReadFile("ActiveMD.kml", kmlContent))
   {
      logger_->warn("ActiveMD.kml not found in KMZ");
      return boost::outcome_v2::failure(
         std::make_error_code(std::errc::io_error));
   }

   logger_->info("Parsing KML ({}) bytes", kmlContent.size());

   try
   {
      MdData data = ParseKml(kmlContent);
      logger_->info("Parsed {} mesoscale discussions",
                    data.discussions_.size());
      return data;
   }
   catch (const std::exception& ex)
   {
      logger_->warn("Failed to parse MD KML: {}", ex.what());
      return boost::outcome_v2::failure(
         std::make_error_code(std::errc::invalid_argument));
   }
}

std::string SpcMdProvider::FetchMdDiscussionText(int mdNumber)
{
   // Construct URL: https://www.spc.noaa.gov/products/md/md0765.html
   std::string url = "https://www.spc.noaa.gov/products/md/md" +
                     fmt::format("{:04d}", mdNumber) + ".html";

   logger_->info("Fetching MD discussion text: {}", url);

   ::cpr::Response r = ::cpr::Get(::cpr::Url {url},
                                  network::cpr::GetHeader(),
                                  network::cpr::GetDefaultTimeout(),
                                  network::cpr::GetDefaultConnectTimeout(),
                                  network::cpr::GetDefaultLowSpeed());

   if (r.status_code != 200)
   {
      logger_->warn(
         "Failed to fetch MD HTML: HTTP {} for URL {}", r.status_code, url);
      return {};
   }

   logger_->debug("Fetched MD HTML ({} bytes)", r.text.size());

   // Parse HTML to extract <pre> content
   htmlDocPtr htmlDoc = htmlReadMemory(r.text.c_str(),
                                       static_cast<int>(r.text.size()),
                                       nullptr,
                                       nullptr,
                                       HTML_PARSE_RECOVER | HTML_PARSE_NOERROR |
                                          HTML_PARSE_NOWARNING);

   if (htmlDoc == nullptr)
   {
      logger_->warn("Failed to parse MD HTML");
      return {};
   }

   std::string result;

   // Recursively search for <pre> tags
   struct PreFinder
   {
      static bool Find(xmlNodePtr node, std::string& output)
      {
         if (node == nullptr)
            return false;

         if (node->type == XML_ELEMENT_NODE &&
             xmlStrcmp(node->name, reinterpret_cast<const xmlChar*>("pre")) ==
                0)
         {
            xmlChar* text = xmlNodeGetContent(node);
            if (text != nullptr)
            {
               output = reinterpret_cast<const char*>(text);
               xmlFree(text);
               return true;
            }
         }

         for (xmlNodePtr child = node->children; child != nullptr;
              child            = child->next)
         {
            if (PreFinder::Find(child, output))
               return true;
         }
         return false;
      }
   };

   // Search from the root
   xmlNodePtr root = xmlDocGetRootElement(htmlDoc);
   if (root != nullptr)
   {
      PreFinder::Find(root, result);
   }

   xmlFreeDoc(htmlDoc);

   if (!result.empty())
   {
      // Trim trailing whitespace
      while (!result.empty() && (result.back() == '\n' ||
                                 result.back() == '\r' || result.back() == ' '))
         result.pop_back();
      logger_->debug("Extracted discussion text ({} chars)", result.size());
   }
   else
   {
      logger_->warn("No <pre> tags found in MD HTML");
   }

   return result;
}

MdData SpcMdProvider::ParseKml(const std::string& kmlContent)
{
   MdData data;
   data.updateTime_ = std::chrono::system_clock::now();

   xmlDocPtr doc =
      xmlParseMemory(kmlContent.c_str(), static_cast<int>(kmlContent.size()));
   if (doc == nullptr)
   {
      logger_->warn("Failed to parse KML XML");
      return data;
   }

   xmlNodePtr root = xmlDocGetRootElement(doc);
   if (root == nullptr)
   {
      xmlFreeDoc(doc);
      return data;
   }

   // Walk: <kml> -> <Document> -> <Folder> -> children
   xmlNodePtr document = nullptr;
   for (xmlNodePtr child = root->children; child != nullptr;
        child            = child->next)
   {
      if (child->type == XML_ELEMENT_NODE &&
          xmlStrcmp(child->name,
                    reinterpret_cast<const xmlChar*>("Document")) == 0)
      {
         document = child;
         break;
      }
   }

   if (document == nullptr)
   {
      xmlFreeDoc(doc);
      return data;
   }

   // Check for <Folder> inside <Document>
   xmlNodePtr folder = nullptr;
   for (xmlNodePtr child = document->children; child != nullptr;
        child            = child->next)
   {
      if (child->type == XML_ELEMENT_NODE &&
          xmlStrcmp(child->name, reinterpret_cast<const xmlChar*>("Folder")) ==
             0)
      {
         folder = child;
         break;
      }
   }

   // Determine which container to iterate:
   //   Folder (if found) - expected NetworkLink entries, or
   //   Document (fallback) - direct Placemark entries
   xmlNodePtr container = (folder != nullptr) ? folder : document;

   static const std::regex mdNumberRegex(R"(MD\s*#?\s*(\d+))",
                                         std::regex::icase);

   for (xmlNodePtr child = container->children; child != nullptr;
        child            = child->next)
   {
      if (child->type != XML_ELEMENT_NODE)
      {
         continue;
      }

      const xmlChar* nodeName = child->name;

      // -------------------------------------------------------
      // 1) NetworkLink processing  (ActiveMD.kml structure)
      // -------------------------------------------------------
      if (xmlStrcmp(nodeName,
                    reinterpret_cast<const xmlChar*>("NetworkLink")) == 0)
      {
         MesoscaleDiscussion md;

         // Extract <name> and <description> from NetworkLink
         for (xmlNodePtr nlChild = child->children; nlChild != nullptr;
              nlChild            = nlChild->next)
         {
            if (nlChild->type != XML_ELEMENT_NODE)
            {
               continue;
            }

            if (xmlStrcmp(nlChild->name,
                          reinterpret_cast<const xmlChar*>("name")) == 0)
            {
               xmlChar* text = xmlNodeGetContent(nlChild);
               if (text != nullptr)
               {
                  md.name_ = reinterpret_cast<const char*>(text);
                  xmlFree(text);

                  std::smatch match;
                  if (std::regex_search(md.name_, match, mdNumberRegex))
                  {
                     md.mdNumber_ = std::stoi(match[1].str());
                  }
               }
            }
            else if (xmlStrcmp(
                        nlChild->name,
                        reinterpret_cast<const xmlChar*>("description")) == 0)
            {
               xmlChar* text = xmlNodeGetContent(nlChild);
               if (text != nullptr)
               {
                  md.description_ = reinterpret_cast<const char*>(text);
                  xmlFree(text);
               }
            }
            else if (xmlStrcmp(nlChild->name,
                               reinterpret_cast<const xmlChar*>("Link")) == 0)
            {
               for (xmlNodePtr linkChild = nlChild->children;
                    linkChild != nullptr;
                    linkChild = linkChild->next)
               {
                  if (linkChild->type == XML_ELEMENT_NODE &&
                      xmlStrcmp(linkChild->name,
                                reinterpret_cast<const xmlChar*>("href")) == 0)
                  {
                     xmlChar* hrefText = xmlNodeGetContent(linkChild);
                     if (hrefText != nullptr)
                     {
                        md.kmlUrl_ = reinterpret_cast<const char*>(hrefText);
                        xmlFree(hrefText);
                     }
                     break;
                  }
               }
            }
         }

         // Try to fetch individual KMZ for polygon data.
         // Use href from KML Link, or construct fallback URL
         if (md.mdNumber_ > 0)
         {
            std::string mdUrl;
            if (!md.kmlUrl_.empty())
            {
               mdUrl = md.kmlUrl_;
               // Upgrade http to https
               if (mdUrl.compare(0, 7, "http://") == 0)
               {
                  mdUrl = "https://" + mdUrl.substr(7);
               }
            }
            else
            {
               mdUrl = "https://www.spc.noaa.gov/products/md/MD" +
                       fmt::format("{:04d}", md.mdNumber_) + ".kmz";
            }

            logger_->info("Fetching MD KMZ from href: {}", mdUrl);

            ::cpr::Response mdR =
               ::cpr::Get(::cpr::Url {mdUrl},
                          network::cpr::GetHeader(),
                          network::cpr::GetDefaultTimeout(),
                          network::cpr::GetDefaultConnectTimeout(),
                          network::cpr::GetDefaultLowSpeed());

            if (mdR.status_code == 200)
            {
               logger_->info("  Fetched MD KMZ ({} bytes)", mdR.text.size());

               // Decompress KMZ via ZipStreamReader
               std::stringstream mdStream;
               mdStream.write(mdR.text.data(),
                              static_cast<std::streamsize>(mdR.text.size()));

               scwx::zip::ZipStreamReader mdReader(mdStream);
               if (mdReader.IsOpen())
               {
                  std::vector<std::string> files = mdReader.ListFiles();

                  for (const auto& internalFile : files)
                  {
                     std::string innerKml;
                     if (mdReader.ReadFile(internalFile, innerKml))
                     {
                        xmlDocPtr innerDoc = xmlParseMemory(
                           innerKml.c_str(), static_cast<int>(innerKml.size()));
                        if (innerDoc != nullptr)
                        {
                           xmlNodePtr innerRoot =
                              xmlDocGetRootElement(innerDoc);
                           if (innerRoot != nullptr)
                           {
                              ParsePlacemarksRecursive(innerRoot, md);
                           }
                           xmlFreeDoc(innerDoc);
                        }
                     }
                  }
               }
            }
            else
            {
               logger_->warn("  Failed to fetch MD KMZ: HTTP {}",
                             mdR.status_code);
            }

            // Fetch full discussion text from HTML page
            if (md.description_.empty() ||
                md.description_.find("<pre>") == std::string::npos)
            {
               std::string fullText = FetchMdDiscussionText(md.mdNumber_);
               if (!fullText.empty())
               {
                  md.description_ = std::move(fullText);
               }
            }

            // Compute centroid from the first ring's coordinates
            if (!md.rings_.empty() && !md.rings_.front().empty())
            {
               double sumLat = 0.0, sumLon = 0.0;
               for (const auto& pt : md.rings_.front())
               {
                  sumLat += pt.first;
                  sumLon += pt.second;
               }
               md.centroid_ = common::Coordinate(
                  sumLat / static_cast<double>(md.rings_.front().size()),
                  sumLon / static_cast<double>(md.rings_.front().size()));
            }

            data.discussions_.push_back(std::move(md));
         }
      }
      // -------------------------------------------------------
      // 2) Direct Placemark processing  (fallback)
      // -------------------------------------------------------
      else if (xmlStrcmp(nodeName,
                         reinterpret_cast<const xmlChar*>("Placemark")) == 0)
      {
         MesoscaleDiscussion md;

         for (xmlNodePtr pmChild = child->children; pmChild != nullptr;
              pmChild            = pmChild->next)
         {
            if (pmChild->type != XML_ELEMENT_NODE)
            {
               continue;
            }

            if (xmlStrcmp(pmChild->name,
                          reinterpret_cast<const xmlChar*>("name")) == 0)
            {
               xmlChar* text = xmlNodeGetContent(pmChild);
               if (text != nullptr)
               {
                  md.name_ = reinterpret_cast<const char*>(text);
                  xmlFree(text);

                  std::smatch match;
                  if (std::regex_search(md.name_, match, mdNumberRegex))
                  {
                     md.mdNumber_ = std::stoi(match[1].str());
                  }
               }
            }
            else if (xmlStrcmp(
                        pmChild->name,
                        reinterpret_cast<const xmlChar*>("description")) == 0)
            {
               xmlChar* text = xmlNodeGetContent(pmChild);
               if (text != nullptr)
               {
                  md.description_ = reinterpret_cast<const char*>(text);
                  xmlFree(text);
               }
            }
         }

         // Extract polygon coordinates using the recursive helper
         ParsePlacemarksRecursive(child, md);

         // Compute centroid from the first ring's coordinates
         if (!md.rings_.empty() && !md.rings_.front().empty())
         {
            double sumLat = 0.0, sumLon = 0.0;
            for (const auto& pt : md.rings_.front())
            {
               sumLat += pt.first;
               sumLon += pt.second;
            }
            md.centroid_ = common::Coordinate(
               sumLat / static_cast<double>(md.rings_.front().size()),
               sumLon / static_cast<double>(md.rings_.front().size()));
         }

         if (md.mdNumber_ > 0)
         {
            data.discussions_.push_back(std::move(md));
         }
      }
   }

   xmlFreeDoc(doc);

   return data;
}

} // namespace scwx::spc
