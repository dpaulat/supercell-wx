#include <scwx/gr/outlook_placefile.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/zip/zip_stream_reader.hpp>

#include <cctype>
#include <charconv>
#include <optional>
#include <sstream>

#include <boost/gil/typedefs.hpp>
#include <boost/json.hpp>
#include <boost/system/error_code.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <fmt/format.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>

namespace scwx
{
namespace gr
{

static const std::string logPrefix_ = "scwx::gr::outlook_placefile";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

namespace
{

// A resolved-color feature ready to write -- the shared intermediate
// representation both ConvertOutlookGeoJsonToPlacefile() and
// ConvertOutlookKmzToPlacefile() reduce their own source formats to, so
// the actual Place File emission (WriteOutlookPlacefile() below) is
// written once, not duplicated per source format.
struct OutlookFeature
{
   boost::gil::rgba8_pixel_t                    color;
   std::vector<std::vector<common::Coordinate>> rings;
};

// Parses a "#RRGGBB" (or "#RGB") hex color string, as SPC's outlook
// GeoJSON provides in each feature's "fill"/"stroke" properties. Returns
// nullopt for anything else (missing, empty, malformed) -- notably
// including the empty string SPC emits on a "no areas" day.
std::optional<boost::gil::rgba8_pixel_t> ParseHexColor(std::string_view hex)
{
   if (!hex.empty() && hex.front() == '#')
   {
      hex.remove_prefix(1);
   }

   std::uint8_t r {};
   std::uint8_t g {};
   std::uint8_t b {};

   if (hex.size() == 6)
   {
      auto parseByte = [&hex](std::size_t offset, std::uint8_t& out) -> bool
      {
         const auto result = std::from_chars(
            hex.data() + offset, hex.data() + offset + 2, out, 16);
         return result.ec == std::errc {};
      };

      if (parseByte(0, r) && parseByte(2, g) && parseByte(4, b))
      {
         return boost::gil::rgba8_pixel_t {r, g, b, 255};
      }
   }

   return std::nullopt;
}

// Parses a KML "aabbggrr" color string (KML's own byte order -- alpha,
// blue, green, red -- the reverse of GeoJSON/CSS's "#rrggbb"; see
// https://developers.google.com/kml/documentation/kmlreference#color).
// Returns nullopt for anything else, same leniency as ParseHexColor.
std::optional<boost::gil::rgba8_pixel_t> ParseKmlColor(std::string_view hex)
{
   std::uint8_t a {};
   std::uint8_t b {};
   std::uint8_t g {};
   std::uint8_t r {};

   if (hex.size() == 8)
   {
      auto parseByte = [&hex](std::size_t offset, std::uint8_t& out) -> bool
      {
         const auto result = std::from_chars(
            hex.data() + offset, hex.data() + offset + 2, out, 16);
         return result.ec == std::errc {};
      };

      if (parseByte(0, a) && parseByte(2, b) && parseByte(4, g) &&
          parseByte(6, r))
      {
         return boost::gil::rgba8_pixel_t {r, g, b, a};
      }
   }

   return std::nullopt;
}

// Emits one contour from a ring of already-resolved coordinates. Per RFC
// 7946, a GeoJSON ring is already explicitly closed (its last position
// repeats its first) -- confirmed live that real WPC QPF KML rings are
// too -- which is exactly the Place File spec's own closing convention
// too (see wxdata/source/scwx/gr/placefile.cpp's Polygon: parsing), so a
// ring's points are emitted as-is, with no extra point appended.
// Appending one anyway double-closes the contour a point early and
// leaves a stray single-point leftover that corrupts every contour after
// it in the same block -- caught by
// OutlookPlacefileTest.ConvertsRealDay1CategoricalOutlook round-tripping
// real SPC data through the actual parser.
void WriteRing(std::ostream& os, const std::vector<common::Coordinate>& ring)
{
   for (const auto& position : ring)
   {
      os << fmt::format(
         "   {:.6f}, {:.6f}\n", position.latitude_, position.longitude_);
   }
}

// One Polygon:/End: block holding every ring as its own contour -- the
// Place File format doesn't distinguish "new sub-polygon" from "hole",
// both are just "the next contour" (see placefile.cpp's Polygon:
// parsing).
void WriteFillBlock(std::ostream&                                       os,
                    const std::vector<std::vector<common::Coordinate>>& rings)
{
   os << "Polygon:\n";
   for (const auto& ring : rings)
   {
      WriteRing(os, ring);
   }
   os << "End:\n\n";
}

// One Line:/End: block per ring -- Place File's Line: is a single
// connected polyline, unlike Polygon:, so each ring needs its own block.
// A ring's points are already closed (see WriteRing), so the polyline's
// last segment connects back to its start on its own, with no special
// handling needed to visually close the loop.
void WriteContourBlocks(
   std::ostream&                                       os,
   const std::vector<std::vector<common::Coordinate>>& rings,
   unsigned int                                        lineWidth)
{
   for (const auto& ring : rings)
   {
      os << fmt::format("Line: {}, 0\n", lineWidth);
      WriteRing(os, ring);
      os << "End:\n\n";
   }
}

// Shared tail for both source formats: Title:/RefreshSeconds:, then one
// Color:/(Polygon:|Line:)/End: block per feature that actually has
// resolved color and rings.
std::string WriteOutlookPlacefile(const std::string& title,
                                  int                refreshSeconds,
                                  const std::vector<OutlookFeature>& features,
                                  OutlookRenderMode                  renderMode,
                                  unsigned int contourLineWidth)
{
   std::ostringstream os;
   os << "Title: " << title << "\n";
   os << "RefreshSeconds: " << refreshSeconds << "\n";
   os << "\n";

   std::size_t featuresWritten = 0;

   for (const auto& feature : features)
   {
      if (feature.rings.empty())
      {
         continue;
      }

      os << fmt::format("Color: {} {} {} {}\n",
                        feature.color[0],
                        feature.color[1],
                        feature.color[2],
                        feature.color[3]);

      if (renderMode == OutlookRenderMode::Fill)
      {
         WriteFillBlock(os, feature.rings);
      }
      else
      {
         WriteContourBlocks(os, feature.rings, contourLineWidth);
      }

      ++featuresWritten;
   }

   if (featuresWritten == 0)
   {
      logger_->info("No outlook polygons for {} (quiet day or parse issue)",
                    title);
   }

   return os.str();
}

// Collects every ring of every polygon in a GeoJSON Polygon or
// MultiPolygon geometry -- a MultiPolygon's sub-polygons and their holes
// all flatten into one flat list of rings, since Fill mode treats "new
// sub-polygon" and "hole" identically anyway (see WriteFillBlock) and
// Contour mode just draws every ring regardless of which polygon or role
// it came from.
std::vector<std::vector<common::Coordinate>>
CollectGeoJsonRings(const boost::json::object& geometry)
{
   std::vector<std::vector<common::Coordinate>> rings;

   const auto& type = geometry.at("type").as_string();

   auto convertRing = [](const boost::json::array& ring)
   {
      std::vector<common::Coordinate> converted;
      converted.reserve(ring.size());
      for (const auto& position : ring)
      {
         const auto&  coords = position.as_array();
         const double lon    = coords.at(0).to_number<double>();
         const double lat    = coords.at(1).to_number<double>();
         converted.emplace_back(lat, lon);
      }
      return converted;
   };

   if (type == "Polygon")
   {
      for (const auto& ring : geometry.at("coordinates").as_array())
      {
         rings.push_back(convertRing(ring.as_array()));
      }
   }
   else if (type == "MultiPolygon")
   {
      for (const auto& polygon : geometry.at("coordinates").as_array())
      {
         for (const auto& ring : polygon.as_array())
         {
            rings.push_back(convertRing(ring.as_array()));
         }
      }
   }
   else
   {
      logger_->warn("Unsupported outlook geometry type: {}",
                    std::string_view {type});
   }

   return rings;
}

// Parses a KML <coordinates> element's text: whitespace-separated
// "lon,lat[,alt]" tuples (the altitude component, if present, is
// ignored -- these outlooks are always 2D). Lenient on malformed tuples
// (skips them) rather than failing the whole ring, matching this file's
// general "skip what can't be used" philosophy for real-world data.
std::vector<common::Coordinate> ParseKmlCoordinates(std::string_view text)
{
   std::vector<common::Coordinate> ring;

   std::size_t pos = 0;
   while (pos < text.size())
   {
      while (pos < text.size() &&
             std::isspace(static_cast<unsigned char>(text[pos])))
      {
         ++pos;
      }
      const std::size_t start = pos;
      while (pos < text.size() &&
             !std::isspace(static_cast<unsigned char>(text[pos])))
      {
         ++pos;
      }
      if (pos == start)
      {
         continue;
      }

      const std::string_view tuple  = text.substr(start, pos - start);
      const std::size_t      comma1 = tuple.find(',');
      if (comma1 == std::string_view::npos)
      {
         continue;
      }
      const std::size_t      comma2 = tuple.find(',', comma1 + 1);
      const std::string_view lonStr = tuple.substr(0, comma1);
      const std::string_view latStr =
         (comma2 == std::string_view::npos) ?
            tuple.substr(comma1 + 1) :
            tuple.substr(comma1 + 1, comma2 - comma1 - 1);

      double     lon {};
      double     lat {};
      const auto lonResult =
         std::from_chars(lonStr.data(), lonStr.data() + lonStr.size(), lon);
      const auto latResult =
         std::from_chars(latStr.data(), latStr.data() + latStr.size(), lat);

      if (lonResult.ec == std::errc {} && latResult.ec == std::errc {})
      {
         ring.emplace_back(lat, lon);
      }
   }

   return ring;
}

// libxml2 XPath querying by local-name() rather than a registered
// namespace prefix -- sidesteps having to bind KML's default namespace
// (xmlns="http://www.opengis.net/kml/2.2") to a prefix just to reach
// otherwise-unqualified element names.
xmlXPathObjectPtr EvalXPath(xmlXPathContextPtr context, const char* expr)
{
   return xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(expr),
                                 context);
}

std::string NodeText(xmlNodePtr node)
{
   std::string text;
   xmlChar*    content = xmlNodeGetContent(node);
   if (content != nullptr)
   {
      text = reinterpret_cast<const char*>(content);
      xmlFree(content);
   }
   return text;
}

// Every <Style id="...">'s own color, read from its <PolyStyle>/<color>
// (LineStyle is deliberately not read -- every real WPC QPF Style here
// carries a PolyStyle, and OutlookRenderMode::Contour derives its line
// color from the same fill color a Fill-mode render would use, matching
// ConvertOutlookGeoJsonToPlacefile()'s own single-color-per-feature
// design).
boost::unordered_flat_map<std::string, boost::gil::rgba8_pixel_t>
CollectKmlStyles(xmlXPathContextPtr context)
{
   boost::unordered_flat_map<std::string, boost::gil::rgba8_pixel_t> styles;

   xmlXPathObjectPtr styleNodes =
      EvalXPath(context, "//*[local-name()='Style']");
   if (styleNodes == nullptr)
   {
      return styles;
   }

   if (styleNodes->nodesetval != nullptr)
   {
      for (int i = 0; i < styleNodes->nodesetval->nodeNr; ++i)
      {
         xmlNodePtr styleNode = styleNodes->nodesetval->nodeTab[i];

         xmlChar* idAttr = xmlGetProp(styleNode, BAD_CAST "id");
         if (idAttr == nullptr)
         {
            continue;
         }
         const std::string id = reinterpret_cast<const char*>(idAttr);
         xmlFree(idAttr);

         for (xmlNodePtr child = styleNode->children; child != nullptr;
              child            = child->next)
         {
            if (child->type != XML_ELEMENT_NODE ||
                std::string_view {reinterpret_cast<const char*>(child->name)} !=
                   "PolyStyle")
            {
               continue;
            }

            for (xmlNodePtr colorNode = child->children; colorNode != nullptr;
                 colorNode            = colorNode->next)
            {
               if (colorNode->type != XML_ELEMENT_NODE ||
                   std::string_view {reinterpret_cast<const char*>(
                      colorNode->name)} != "color")
               {
                  continue;
               }

               if (auto color = ParseKmlColor(NodeText(colorNode)))
               {
                  styles.emplace(id, *color);
               }
            }
         }
      }
   }

   xmlXPathFreeObject(styleNodes);
   return styles;
}

} // namespace

std::string ConvertOutlookGeoJsonToPlacefile(
   const std::string&                 geoJson,
   const std::string&                 title,
   int                                refreshSeconds,
   const std::vector<OutlookDnColor>& dnColorTable,
   OutlookRenderMode                  renderMode,
   unsigned int                       contourLineWidth)
{
   boost::system::error_code ec;
   const boost::json::value  parsed = boost::json::parse(geoJson, ec);

   if (ec || !parsed.is_object() ||
       parsed.as_object().if_contains("features") == nullptr)
   {
      logger_->warn(
         "Could not parse outlook GeoJSON for {}: {}", title, ec.message());
      return {};
   }

   std::vector<OutlookFeature> features;

   for (const auto& feature : parsed.at("features").as_array())
   {
      const auto& properties = feature.at("properties").as_object();

      // Prefer fill (matches what SPC's own map renders); fall back to
      // stroke if a source only provides an outline color. Either being
      // empty/absent (SPC's "no areas" day) means skip -- there is
      // nothing meaningful to draw.
      std::optional<boost::gil::rgba8_pixel_t> color;
      if (const auto* fill = properties.if_contains("fill");
          fill != nullptr && fill->is_string())
      {
         color = ParseHexColor(fill->as_string());
      }
      if (!color)
      {
         if (const auto* stroke = properties.if_contains("stroke");
             stroke != nullptr && stroke->is_string())
         {
            color = ParseHexColor(stroke->as_string());
         }
      }

      if (!color)
      {
         if (const auto* dn = properties.if_contains("dn");
             dn != nullptr && dn->is_number())
         {
            const auto dnValue = dn->to_number<int>();
            for (const auto& entry : dnColorTable)
            {
               if (entry.dn == dnValue)
               {
                  color = entry.color;
                  break;
               }
            }
         }
      }

      if (!color)
      {
         continue;
      }

      auto rings = CollectGeoJsonRings(feature.at("geometry").as_object());
      if (rings.empty())
      {
         continue;
      }

      features.push_back({*color, std::move(rings)});
   }

   return WriteOutlookPlacefile(
      title, refreshSeconds, features, renderMode, contourLineWidth);
}

std::string ConvertOutlookKmzToPlacefile(const std::string& kmzBytes,
                                         const std::string& title,
                                         int                refreshSeconds,
                                         OutlookRenderMode  renderMode,
                                         unsigned int       contourLineWidth)
{
   std::istringstream   kmzStream {kmzBytes};
   zip::ZipStreamReader zipReader {kmzStream};

   std::string kml;
   if (!zipReader.IsOpen() || !zipReader.ReadFile("doc.kml", kml))
   {
      logger_->warn("Could not read doc.kml from KMZ for {}", title);
      return {};
   }

   xmlDocPtr doc = xmlReadMemory(
      kml.data(), static_cast<int>(kml.size()), nullptr, nullptr, 0);
   if (doc == nullptr)
   {
      logger_->warn("Could not parse doc.kml as XML for {}", title);
      return {};
   }

   xmlXPathContextPtr context = xmlXPathNewContext(doc);
   if (context == nullptr)
   {
      xmlFreeDoc(doc);
      logger_->warn("Could not create XPath context for {}", title);
      return {};
   }

   const auto styles = CollectKmlStyles(context);

   std::vector<OutlookFeature> features;

   xmlXPathObjectPtr placemarkNodes =
      EvalXPath(context, "//*[local-name()='Placemark']");
   if (placemarkNodes != nullptr && placemarkNodes->nodesetval != nullptr)
   {
      for (int i = 0; i < placemarkNodes->nodesetval->nodeNr; ++i)
      {
         xmlNodePtr placemark = placemarkNodes->nodesetval->nodeTab[i];

         std::optional<boost::gil::rgba8_pixel_t>     color;
         std::vector<std::vector<common::Coordinate>> rings;

         for (xmlNodePtr child = placemark->children; child != nullptr;
              child            = child->next)
         {
            if (child->type != XML_ELEMENT_NODE)
            {
               continue;
            }
            const std::string_view name {
               reinterpret_cast<const char*>(child->name)};

            if (name == "styleUrl")
            {
               std::string styleId = NodeText(child);
               if (!styleId.empty() && styleId.front() == '#')
               {
                  styleId.erase(0, 1);
               }
               if (const auto it = styles.find(styleId); it != styles.end())
               {
                  color = it->second;
               }
            }
         }

         // <LinearRing> may sit under <outerBoundaryIs> or
         // <innerBoundaryIs>, directly under <Polygon>, or nested inside
         // <MultiGeometry> -- rather than walking that structure
         // explicitly, an XPath relative to this Placemark reaches every
         // ring regardless of depth or role, matching
         // CollectGeoJsonRings()'s own "flatten everything into
         // contours" philosophy.
         xmlXPathSetContextNode(placemark, context);
         xmlXPathObjectPtr ringNodes = EvalXPath(
            context,
            ".//*[local-name()='LinearRing']/*[local-name()='coordinates']");
         if (ringNodes != nullptr && ringNodes->nodesetval != nullptr)
         {
            for (int j = 0; j < ringNodes->nodesetval->nodeNr; ++j)
            {
               auto ring = ParseKmlCoordinates(
                  NodeText(ringNodes->nodesetval->nodeTab[j]));
               if (!ring.empty())
               {
                  rings.push_back(std::move(ring));
               }
            }
         }
         if (ringNodes != nullptr)
         {
            xmlXPathFreeObject(ringNodes);
         }

         if (color && !rings.empty())
         {
            features.push_back({*color, std::move(rings)});
         }
      }
   }
   if (placemarkNodes != nullptr)
   {
      xmlXPathFreeObject(placemarkNodes);
   }

   xmlXPathFreeContext(context);
   xmlFreeDoc(doc);

   return WriteOutlookPlacefile(
      title, refreshSeconds, features, renderMode, contourLineWidth);
}

const std::vector<OutlookDnColor> kWpcEroColorTable {
   {1, boost::gil::rgba8_pixel_t {0, 255, 0, 117}},    // Marginal
   {2, boost::gil::rgba8_pixel_t {255, 255, 0, 117}},  // Slight
   {3, boost::gil::rgba8_pixel_t {238, 44, 44, 117}},  // Moderate
   {4, boost::gil::rgba8_pixel_t {255, 0, 255, 117}}}; // High

} // namespace gr
} // namespace scwx
