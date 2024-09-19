#pragma once

#if defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wdelete-non-abstract-non-virtual-dtor"
#endif

#include <scwx/awips/phenomenon.hpp>

#if defined(__clang__)
#   pragma clang diagnostic pop
#endif

#include <scwx/qt/map/draw_layer.hpp>
#include <scwx/qt/types/text_event_key.hpp>

#include <memory>
#include <string>
#include <vector>

namespace scwx
{
namespace qt
{
namespace map
{

class AlertLayer : public DrawLayer
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(AlertLayer)

public:
   explicit AlertLayer(std::shared_ptr<MapContext> context,
                       scwx::awips::Phenomenon     phenomenon);
   ~AlertLayer();

   void Initialize() override final;
   void Render(const QMapLibre::CustomLayerRenderParameters&) override final;
   void Deinitialize() override final;

   static void InitializeHandler();

signals:
   void AlertSelected(const types::TextEventKey& key);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
