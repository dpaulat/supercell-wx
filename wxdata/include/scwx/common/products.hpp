#pragma once

#include <scwx/util/iterator.hpp>

#include <string>

namespace scwx
{
namespace common
{

enum class RadarProductGroup
{
   Level2,
   Level3,
   Unknown
};
typedef util::Iterator<RadarProductGroup,
                       RadarProductGroup::Level2,
                       RadarProductGroup::Level3>
   RadarProductGroupIterator;

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

enum class Level3ProductCategory
{
   Reflectivity,
   Velocity,
   StormRelativeVelocity,
   SpectrumWidth,
   DifferentialReflectivity,
   SpecificDifferentialPhase,
   CorrelationCoefficient,
   Unknown
};
typedef util::Iterator<Level3ProductCategory,
                       Level3ProductCategory::Reflectivity,
                       Level3ProductCategory::CorrelationCoefficient>
   Level3ProductCategoryIterator;

const std::string& GetRadarProductGroupName(RadarProductGroup group);
RadarProductGroup  GetRadarProductGroup(const std::string& name);

const std::string& GetLevel2Name(Level2Product product);
const std::string& GetLevel2Description(Level2Product product);
const std::string& GetLevel2Palette(Level2Product product);
Level2Product      GetLevel2Product(const std::string& name);

const std::string& GetLevel3CategoryName(Level3ProductCategory category);
const std::string& GetLevel3CategoryDescription(Level3ProductCategory category);
const std::string&
GetLevel3CategoryDefaultProduct(Level3ProductCategory category);
Level3ProductCategory GetLevel3Category(const std::string& categoryName);
Level3ProductCategory
                   GetLevel3CategoryByProduct(const std::string& productName);
const std::string& GetLevel3Palette(int16_t productCode);

} // namespace common
} // namespace scwx
