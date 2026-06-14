#pragma once

#include <scwx/awips/phenomenon.hpp>

#include <scwx/qt/map/draw_layer.hpp>
#include <scwx/qt/types/text_event_key.hpp>

#include <memory>

namespace scwx::qt::map
{

class AlertLayer : public DrawLayer
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(AlertLayer)

public:
   explicit AlertLayer(const std::shared_ptr<render::RenderContext>& renderContext,
                       scwx::awips::Phenomenon phenomenon);
   ~AlertLayer();

   void Initialize(const std::shared_ptr<MapContext>& mapContext) final;
   void Render(const std::shared_ptr<MapContext>& mapContext,
               const QMapLibre::CustomLayerRenderParameters&) final;
   void Deinitialize() final;

   static void InitializeHandler();

signals:
   void AlertSelected(const types::TextEventKey& key);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map
