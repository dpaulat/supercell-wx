#pragma once

#include <scwx/awips/phenomenon.hpp>
#include <scwx/util/iterator.hpp>

#include <array>
#include <string>
#include <variant>

#include <boost/container/stable_vector.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>

namespace scwx
{
namespace qt
{
namespace types
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
   RadarRange,
   Unknown
};
typedef scwx::util::
   Iterator<DataLayer, DataLayer::RadarRange, DataLayer::RadarRange>
      DataLayerIterator;

enum class InformationLayer
{
   MapOverlay,
   ColorTable,
   Unknown
};

enum class MapLayer
{
   MapSymbology,
   MapUnderlay,
   Unknown
};

typedef std::variant<std::monostate,
                     DataLayer,
                     InformationLayer,
                     MapLayer,
                     awips::Phenomenon,
                     std::string>
   LayerDescription;

struct LayerInfo
{
   LayerType           type_;
   LayerDescription    description_;
   bool                movable_ {true};
   std::array<bool, 4> displayed_ {true, true, true, true};
};

typedef boost::container::stable_vector<LayerInfo> LayerVector;

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

} // namespace types
} // namespace qt
} // namespace scwx
