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

class AlertLayerImpl;

class AlertLayer
{
public:
   explicit AlertLayer(std::shared_ptr<MapContext> context);
   ~AlertLayer();

   std::vector<std::string> AddLayers(awips::Phenomenon  phenomenon,
                                      const std::string& before = {});

private:
   std::unique_ptr<AlertLayerImpl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
