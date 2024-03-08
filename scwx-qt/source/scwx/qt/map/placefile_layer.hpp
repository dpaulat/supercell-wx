#pragma once

#include <scwx/qt/map/draw_layer.hpp>

#include <string>

namespace scwx
{
namespace qt
{
namespace map
{

class PlacefileLayer : public DrawLayer
{
   Q_OBJECT

public:
   explicit PlacefileLayer(const std::shared_ptr<MapContext>& context,
                           const std::string&                 placefileName);
   ~PlacefileLayer();

   std::string placefile_name() const;

   void set_placefile_name(const std::string& placefileName);

   void Initialize() override final;
   void Render(const QMapLibre::CustomLayerRenderParameters&) override final;
   void Deinitialize() override final;

   void ReloadData();

signals:
   void DataReloaded();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
