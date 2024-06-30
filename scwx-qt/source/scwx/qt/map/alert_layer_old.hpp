#pragma once

#include <scwx/awips/phenomenon.hpp>
#include <scwx/qt/map/map_context.hpp>

#include <memory>
#include <string>
#include <vector>

namespace scwx
{
namespace qt
{
namespace map
{

class AlertLayerOldImpl;

class AlertLayerOld
{
public:
   explicit AlertLayerOld(std::shared_ptr<MapContext> context);
   ~AlertLayerOld();

   std::vector<std::string> AddLayers(awips::Phenomenon  phenomenon,
                                      const std::string& before = {});

private:
   std::unique_ptr<AlertLayerOldImpl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
