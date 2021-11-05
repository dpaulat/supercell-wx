#pragma once

#include <scwx/util/iterator.hpp>

#include <string>

namespace scwx
{
namespace common
{

const std::string LEVEL2_GROUP_ID = "L2";

enum class Level2Product
{
   Reflectivity,
   Velocity,
   SpectrumWidth,
   DifferentialReflectivity,
   DifferentialPhase,
   CorrelationCoefficient,
   ClutterFilterPowerRemoved,
   Unknown
};
typedef util::Iterator<Level2Product,
                       Level2Product::Reflectivity,
                       Level2Product::ClutterFilterPowerRemoved>
   Level2ProductIterator;

const std::string&  GetLevel2Name(Level2Product product);
const std::string&  GetLevel2Description(Level2Product product);
const Level2Product GetLevel2Product(const std::string& id);

} // namespace common
} // namespace scwx
