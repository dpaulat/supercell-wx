#include <scwx/qt/types/layer_types.hpp>

#include <unordered_map>

#include <boost/algorithm/string.hpp>
#include <boost/json.hpp>

namespace scwx
{
namespace qt
{
namespace types
{

static const std::unordered_map<LayerType, std::string> layerTypeName_ {
   {LayerType::Map, "Map"},
   {LayerType::Radar, "Radar"},
   {LayerType::Alert, "Alert"},
   {LayerType::Placefile, "Placefile"},
   {LayerType::Information, "Information"},
   {LayerType::Data, "Data"},
   {LayerType::Unknown, "?"}};

static const std::unordered_map<DataLayer, std::string> dataLayerName_ {
   {DataLayer::RadarRange, "Radar Range"}, {DataLayer::Unknown, "?"}};

static const std::unordered_map<InformationLayer, std::string>
   informationLayerName_ {{InformationLayer::MapOverlay, "Map Overlay"},
                          {InformationLayer::ColorTable, "Color Table"},
                          {InformationLayer::Unknown, "?"}};

static const std::unordered_map<MapLayer, std::string> mapLayerName_ {
   {MapLayer::MapSymbology, "Map Symbology"},
   {MapLayer::MapUnderlay, "Map Underlay"},
   {MapLayer::Unknown, "?"}};

static const std::string kTypeName_ {"type"};
static const std::string kDescriptionName_ {"description"};
static const std::string kMovableName_ {"movable"};
static const std::string kDisplayedName_ {"displayed"};

LayerType GetLayerType(const std::string& name)
{
   auto result =
      std::find_if(layerTypeName_.cbegin(),
                   layerTypeName_.cend(),
                   [&](const std::pair<LayerType, std::string>& pair) -> bool
                   { return boost::iequals(pair.second, name); });

   if (result != layerTypeName_.cend())
   {
      return result->first;
   }
   else
   {
      return LayerType::Unknown;
   }
}

std::string GetLayerTypeName(LayerType layerType)
{
   return layerTypeName_.at(layerType);
}

DataLayer GetDataLayer(const std::string& name)
{
   auto result =
      std::find_if(dataLayerName_.cbegin(),
                   dataLayerName_.cend(),
                   [&](const std::pair<DataLayer, std::string>& pair) -> bool
                   { return boost::iequals(pair.second, name); });

   if (result != dataLayerName_.cend())
   {
      return result->first;
   }
   else
   {
      return DataLayer::Unknown;
   }
}

std::string GetDataLayerName(DataLayer layer)
{
   return dataLayerName_.at(layer);
}

InformationLayer GetInformationLayer(const std::string& name)
{
   auto result = std::find_if(
      informationLayerName_.cbegin(),
      informationLayerName_.cend(),
      [&](const std::pair<InformationLayer, std::string>& pair) -> bool
      { return boost::iequals(pair.second, name); });

   if (result != informationLayerName_.cend())
   {
      return result->first;
   }
   else
   {
      return InformationLayer::Unknown;
   }
}

std::string GetInformationLayerName(InformationLayer layer)
{
   return informationLayerName_.at(layer);
}

MapLayer GetMapLayer(const std::string& name)
{
   auto result =
      std::find_if(mapLayerName_.cbegin(),
                   mapLayerName_.cend(),
                   [&](const std::pair<MapLayer, std::string>& pair) -> bool
                   { return boost::iequals(pair.second, name); });

   if (result != mapLayerName_.cend())
   {
      return result->first;
   }
   else
   {
      return MapLayer::Unknown;
   }
}

std::string GetMapLayerName(MapLayer layer)
{
   return mapLayerName_.at(layer);
}

void tag_invoke(boost::json::value_from_tag,
                boost::json::value& jv,
                const LayerInfo&    record)
{
   std::string description {};

   if (std::holds_alternative<awips::Phenomenon>(record.description_))
   {
      description = awips::GetPhenomenonCode(
         std::get<awips::Phenomenon>(record.description_));
   }
   else if (std::holds_alternative<DataLayer>(record.description_))
   {
      description = GetDataLayerName(std::get<DataLayer>(record.description_));
   }
   else if (std::holds_alternative<InformationLayer>(record.description_))
   {
      description = GetInformationLayerName(
         std::get<InformationLayer>(record.description_));
   }
   else if (std::holds_alternative<MapLayer>(record.description_))
   {
      description = GetMapLayerName(std::get<MapLayer>(record.description_));
   }
   else if (std::holds_alternative<std::string>(record.description_))
   {
      description = std::get<std::string>(record.description_);
   }

   jv = {{kTypeName_, GetLayerTypeName(record.type_)},
         {kDescriptionName_, description},
         {kMovableName_, record.movable_},
         {kDisplayedName_, boost::json::value_from(record.displayed_)}};
}

LayerInfo tag_invoke(boost::json::value_to_tag<LayerInfo>,
                     const boost::json::value& jv)
{
   const LayerType layerType =
      GetLayerType(boost::json::value_to<std::string>(jv.at(kTypeName_)));
   const std::string descriptionName =
      boost::json::value_to<std::string>(jv.at(kDescriptionName_));

   LayerDescription description {};

   if (layerType == LayerType::Map)
   {
      description = GetMapLayer(descriptionName);
   }
   else if (layerType == LayerType::Information)
   {
      description = GetInformationLayer(descriptionName);
   }
   else if (layerType == LayerType::Data)
   {
      description = GetDataLayer(descriptionName);
   }
   else if (layerType == LayerType::Radar)
   {
      description = std::monostate {};
   }
   else if (layerType == LayerType::Alert)
   {
      description = awips::GetPhenomenon(descriptionName);
   }
   else
   {
      description = descriptionName;
   }

   return LayerInfo {
      layerType,
      description,
      jv.at(kMovableName_).as_bool(),
      boost::json::value_to<std::array<bool, 4>>(jv.at(kDisplayedName_))};
}

} // namespace types
} // namespace qt
} // namespace scwx
