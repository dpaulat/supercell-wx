#pragma once

#include <scwx/qt/gl/draw/draw_item.hpp>
#include <scwx/qt/gl/gl_context.hpp>
#include <scwx/qt/map/map_annotation_types.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace scwx::qt::map
{
class MapAnnotationModel;
}

namespace scwx::qt::gl::draw
{

class MapAnnotationsDrawItem : public DrawItem
{
public:
   explicit MapAnnotationsDrawItem(std::shared_ptr<GlContext> context,
                                   map::MapAnnotationModel*   model);
   ~MapAnnotationsDrawItem() override;

   MapAnnotationsDrawItem(const MapAnnotationsDrawItem&)            = delete;
   MapAnnotationsDrawItem& operator=(const MapAnnotationsDrawItem&) = delete;
   MapAnnotationsDrawItem(MapAnnotationsDrawItem&&)                 = delete;
   MapAnnotationsDrawItem& operator=(MapAnnotationsDrawItem&&)      = delete;

   void Initialize() override;
   void Render(const QMapLibre::CustomLayerRenderParameters& params) override;
   void Deinitialize() override;

   void SetPreviewPolyline(const std::vector<common::Coordinate>& pts,
                           const map::MapAnnotationStyle&         style,
                           bool roundStroke = false);
   void ClearPreview();

   void Rebuild();

   [[nodiscard]] std::vector<std::uint64_t>
   PickObjects(const glm::vec2&          mouseMapCoords,
               const common::Coordinate& mouseGeo) const;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::gl::draw
