#pragma once

#include <scwx/awips/phenomenon.hpp>
#include <scwx/qt/types/map_types.hpp>
#include <scwx/util/iterator.hpp>

#include <array>
#include <string>
#include <variant>

#include <boost/container/stable_vector.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>

namespace scwx::qt::types
{

enum class LayerType
{
   Map,
   Radar,
   Alert,
   Placefile,
   Information,
   Data,
   Unknown
};

enum class DataLayer
{
   OverlayProduct,
   RadarRange,
   Unknown
};
using DataLayerIterator = scwx::util::
   Iterator<DataLayer, DataLayer::OverlayProduct, DataLayer::RadarRange>;

enum class InformationLayer
{
   MapOverlay,
   RadarSite,
   ColorTable,
   Markers,
   Unknown
};

enum class MapLayer
{
   MapSymbology,
   MapUnderlay,
   Unknown
};

using LayerDescription = std::variant<std::monostate,
                                      DataLayer,
                                      InformationLayer,
                                      MapLayer,
                                      awips::Phenomenon,
                                      std::string>;

struct LayerInfo
{
   LayerType                    type_ {LayerType::Unknown};
   LayerDescription             description_;
   bool                         movable_ {true};
   std::array<bool, kMapCount_> displayed_ {
      true, true, true, true, true, true, true, true, true};
};

using LayerVector = boost::container::stable_vector<LayerInfo>;

LayerType   GetLayerType(const std::string& name);
std::string GetLayerTypeName(LayerType layerType);

DataLayer   GetDataLayer(const std::string& name);
std::string GetDataLayerName(DataLayer layer);

InformationLayer GetInformationLayer(const std::string& name);
std::string      GetInformationLayerName(InformationLayer layer);

MapLayer    GetMapLayer(const std::string& name);
std::string GetMapLayerName(MapLayer layer);

std::string GetLayerDescriptionName(LayerDescription description);

std::string GetLayerName(LayerType type, LayerDescription description);

void      tag_invoke(boost::json::value_from_tag,
                     boost::json::value& jv,
                     const LayerInfo&    record);
LayerInfo tag_invoke(boost::json::value_to_tag<LayerInfo>,
                     const boost::json::value& jv);

} // namespace scwx::qt::types
