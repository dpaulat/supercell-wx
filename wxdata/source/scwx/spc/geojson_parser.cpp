#include <scwx/spc/geojson_parser.hpp>
#include <scwx/spc/spc_types.hpp>

#include <boost/json.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace scwx::spc
{

namespace
{

std::vector<std::pair<double, double>>
ParseCoordinateRing(const boost::json::array& coords)
{
   std::vector<std::pair<double, double>> ring;
   ring.reserve(coords.size());

   for (const auto& pt : coords)
   {
      if (pt.is_array() && pt.as_array().size() >= 2)
      {
         double lon = pt.as_array()[0].as_double();
         double lat = pt.as_array()[1].as_double();
         ring.emplace_back(lat, lon);
      }
   }
   return ring;
}

} // namespace

OutlookData ParseSpcGeoJson(OutlookDay         day,
                            OutlookProduct     product,
                            const std::string& jsonBody)
{
   OutlookData data;
   data.day_     = day;
   data.product_ = product;

   boost::json::value parsed = boost::json::parse(jsonBody);

   if (!parsed.is_object())
   {
      return data;
   }

   const auto& obj = parsed.as_object();

   // Expect a FeatureCollection
   auto typeIt = obj.find("type");
   if (typeIt == obj.end() ||
       typeIt->value().as_string() != "FeatureCollection")
   {
      return data;
   }

   auto featuresIt = obj.find("features");
   if (featuresIt == obj.end() || !featuresIt->value().is_array())
   {
      return data;
   }

   bool isProbability = (product != OutlookProduct::Categorical);

   const auto& features = featuresIt->value().as_array();
   data.polygons_.reserve(features.size());

   for (const auto& feature : features)
   {
      if (!feature.is_object())
      {
         continue;
      }

      const auto& featObj = feature.as_object();

      // Extract properties from GeoJSON
      int32_t     dn = 0;
      std::string fill;
      std::string stroke;
      int         cigLevel = 0;
      auto        propsIt  = featObj.find("properties");
      if (propsIt != featObj.end() && propsIt->value().is_object())
      {
         const auto& props = propsIt->value().as_object();
         auto        dnIt  = props.find("DN");
         if (dnIt != props.end() && dnIt->value().is_int64())
         {
            dn = static_cast<int32_t>(dnIt->value().as_int64());
         }

         auto fillIt = props.find("fill");
         if (fillIt != props.end() && fillIt->value().is_string())
         {
            fill = std::string(fillIt->value().as_string());
         }

         auto strokeIt = props.find("stroke");
         if (strokeIt != props.end() && strokeIt->value().is_string())
         {
            stroke = std::string(strokeIt->value().as_string());
         }

         // Detect CIG level from LABEL (e.g. "CIG1", "CIG2", "CIG3")
         auto labelIt = props.find("LABEL");
         if (labelIt != props.end() && labelIt->value().is_string())
         {
            std::string label(labelIt->value().as_string());
            if (label.size() > 3 && label.rfind("CIG", 0) == 0)
            {
               // Extract the digit after "CIG"
               char cigChar = label[3];
               if (cigChar >= '1' && cigChar <= '3')
               {
                  cigLevel = cigChar - '0';
               }
            }
         }
      }

      // Extract geometry
      auto geomIt = featObj.find("geometry");
      if (geomIt == featObj.end() || !geomIt->value().is_object())
      {
         continue;
      }

      const auto& geomObj = geomIt->value().as_object();

      auto geomTypeIt = geomObj.find("type");
      if (geomTypeIt == geomObj.end())
      {
         continue;
      }

      std::string geomType(geomTypeIt->value().as_string());

      auto coordsIt = geomObj.find("coordinates");
      if (coordsIt == geomObj.end() || !coordsIt->value().is_array())
      {
         continue;
      }

      const auto& coords = coordsIt->value().as_array();

      OutlookPolygon polygon;
      polygon.dn_              = dn;
      polygon.isProbability_   = isProbability;
      polygon.categoricalRisk_ = GetCategoricalRisk(dn);
      polygon.fillColor_       = fill;
      polygon.strokeColor_     = stroke;
      polygon.cigLevel_        = cigLevel;

      if (geomType == "Polygon")
      {
         // coordinates: [[ring1], [ring2], ...]
         for (const auto& ring : coords)
         {
            if (ring.is_array())
            {
               polygon.rings_.push_back(ParseCoordinateRing(ring.as_array()));
            }
         }
      }
      else if (geomType == "MultiPolygon")
      {
         // coordinates: [[[ring1], [ring2]], ...]
         for (const auto& polygonCoords : coords)
         {
            if (polygonCoords.is_array())
            {
               for (const auto& ring : polygonCoords.as_array())
               {
                  if (ring.is_array())
                  {
                     polygon.rings_.push_back(
                        ParseCoordinateRing(ring.as_array()));
                  }
               }
            }
         }
      }

      if (!polygon.rings_.empty())
      {
         data.polygons_.push_back(std::move(polygon));
      }
   }

   return data;
}

} // namespace scwx::spc
