#include <scwx/qt/gl/draw/map_annotations_draw_item.hpp>
#include <scwx/qt/map/map_annotation_layer.hpp>
#include <scwx/qt/map/map_annotation_model.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/qt/util/maplibre.hpp>

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include <QMapLibre/Map>
#include <QMapLibre/Utils>
#include <QPointF>
#include <QWidget>
#include <glm/gtc/type_ptr.hpp>

namespace scwx::qt::map
{

namespace
{
constexpr double kMeasureHandlePixels {12.0};
constexpr double kMeasureHandleStrokeWidthMultiplier {2.0};
constexpr double kMeasurementAnchorRatio {0.5};
constexpr double kMinimumCircleRadiusM {10.0};

enum class MeasureHandle : std::uint8_t
{
   A,
   B
};

struct MeasureHandleSelection
{
   std::uint64_t id {};
   MeasureHandle handle {MeasureHandle::A};
};

/** Sample the map along a screen-space segment so fast drags still get dense
 * geo points. */
void AppendFreehandAlongPixelSegment(std::vector<common::Coordinate>&       pts,
                                     const std::shared_ptr<QMapLibre::Map>& map,
                                     const QPointF& fromPx,
                                     const QPointF& toPx)
{
   const double dx  = toPx.x() - fromPx.x();
   const double dy  = toPx.y() - fromPx.y();
   const double len = std::hypot(dx, dy);
   // Small step: path already uniform in screen space; do not dedupe by ground
   // meters — at high zoom 2px << 0.06m and intermediates were dropped →
   // dotted.
   constexpr double kPixelStep {1.5};
   const int steps = std::max(1, static_cast<int>(std::ceil(len / kPixelStep)));
   for (int i = 1; i <= steps; ++i)
   {
      const double  t = static_cast<double>(i) / static_cast<double>(steps);
      const QPointF px(fromPx.x() + dx * t, fromPx.y() + dy * t);
      const auto    c = map->coordinateForPixel(px);
      const common::Coordinate g {c.first, c.second};
      if (pts.empty())
      {
         pts.push_back(g);
         continue;
      }
      const auto& back = pts.back();
      if (back.latitude_ == g.latitude_ && back.longitude_ == g.longitude_)
      {
         continue;
      }
      pts.push_back(g);
   }
}

double CoordinateGapM(const common::Coordinate& a, const common::Coordinate& b)
{
   return util::GeographicLib::GetDistance(
             a.latitude_, a.longitude_, b.latitude_, b.longitude_)
      .value();
}

void SimplifyFreehandPoints(std::vector<common::Coordinate>& pts,
                            double                           toleranceM)
{
   if (pts.size() < 3 || toleranceM <= 0.0)
   {
      return;
   }

   std::vector<common::Coordinate> simplified;
   simplified.reserve(pts.size());
   simplified.push_back(pts.front());

   for (std::size_t i = 1; i + 1 < pts.size(); ++i)
   {
      if (CoordinateGapM(simplified.back(), pts[i]) >= toleranceM)
      {
         simplified.push_back(pts[i]);
      }
   }

   if (CoordinateGapM(simplified.back(), pts.back()) > 0.0)
   {
      simplified.push_back(pts.back());
   }

   pts.swap(simplified);
}

units::length::meters<double>
MeasureDistanceM(const MapAnnotationMeasure& measure)
{
   return units::length::meters<double> {
      util::GeographicLib::GetDistance(measure.a.latitude_,
                                       measure.a.longitude_,
                                       measure.b.latitude_,
                                       measure.b.longitude_)
         .value()};
}
} // namespace

class MapAnnotationLayer::Impl
{
public:
   explicit Impl(std::shared_ptr<render::RenderContext> renderContext) :
       renderContext_ {std::move(renderContext)},
       model_ {},
       draw_ {std::make_shared<gl::draw::MapAnnotationsDrawItem>(renderContext_,
                                                                 &model_)}
   {
   }

   std::shared_ptr<MapContext>                       mapContext_;
   std::shared_ptr<render::RenderContext>            renderContext_;
   MapAnnotationModel                                model_;
   std::shared_ptr<gl::draw::MapAnnotationsDrawItem> draw_;

   MapAnnotationTool  tool_ {MapAnnotationTool::None};
   MapAnnotationStyle style_ {};
   bool               visible_ {true};

   bool                                         drawing_ {false};
   std::vector<common::Coordinate>              draftPoints_ {};
   common::Coordinate                           pressGeo_ {};
   std::optional<common::Coordinate>            circleCenter_ {};
   std::optional<common::Coordinate>            rectCorner_ {};
   std::optional<common::Coordinate>            measureA_ {};
   std::optional<units::length::meters<double>> lastMeasureM_ {};
   std::optional<MeasureHandleSelection>        draggedMeasureHandle_ {};
   /** Last pointer position (widget px) while freehand drawing; drives pixel
    * resampling. */
   std::optional<QPointF>            lastFreehandPixel_ {};
   std::optional<QPointF>            lastErasePixel_ {};
   std::unordered_set<std::uint64_t> erasedIds_ {};
   std::unordered_set<std::uint64_t> pendingGpuEraseIds_ {};
   bool                              eraseGpuDirty_ {false};

   void EmitMeasureUpdated(MapAnnotationLayer*           self,
                           units::length::meters<double> distanceM)
   {
      lastMeasureM_ = distanceM;
      Q_EMIT self->MeasureUpdated(distanceM.value());
   }

   void CancelInteraction()
   {
      drawing_ = false;
      measureA_.reset();
      draftPoints_.clear();
      circleCenter_.reset();
      rectCorner_.reset();
      draggedMeasureHandle_.reset();
      lastFreehandPixel_.reset();
      lastErasePixel_.reset();
      erasedIds_.clear();
      pendingGpuEraseIds_.clear();
      eraseGpuDirty_ = false;
      draw_->ClearPreview();
   }

   void CommitPolyline(bool roundStroke)
   {
      if (draftPoints_.empty() || (draftPoints_.size() < 2 && !roundStroke))
      {
         draftPoints_.clear();
         return;
      }
      MapAnnotationPolyline pl;
      pl.points = draftPoints_;
      if (roundStroke)
      {
         const double toleranceM =
            std::clamp(style_.strokeWidthM.value() * 0.04, 2.0, 30.0);
         SimplifyFreehandPoints(pl.points, toleranceM);
      }
      pl.roundStroke = roundStroke;

      MapAnnotationObject obj {};
      obj.payload = std::move(pl);
      obj.style   = style_;
      static_cast<void>(model_.Add(std::move(obj)));
      draftPoints_.clear();
      draw_->Rebuild();
      draw_->ClearPreview();
   }

   void UpdatePreview()
   {
      if (draftPoints_.empty())
      {
         draw_->ClearPreview();
         return;
      }
      if (tool_ == MapAnnotationTool::Measure)
      {
         draw_->SetPreviewPolyline(draftPoints_, style_, true);
         return;
      }
      if (draftPoints_.size() < 2 && tool_ != MapAnnotationTool::Freehand)
      {
         draw_->ClearPreview();
         return;
      }
      draw_->SetPreviewPolyline(
         draftPoints_, style_, tool_ == MapAnnotationTool::Freehand);
   }

   [[nodiscard]] double
   MeasureHandleToleranceM(const std::shared_ptr<QMapLibre::Map>& map,
                           double latitude) const
   {
      return std::max(
         QMapLibre::metersPerPixelAtLatitude(latitude, map->zoom()) *
            kMeasureHandlePixels,
         style_.strokeWidthM.value() * kMeasureHandleStrokeWidthMultiplier);
   }

   [[nodiscard]] std::optional<MeasureHandleSelection>
   FindMeasureHandle(const std::shared_ptr<QMapLibre::Map>& map,
                     const common::Coordinate&              geo) const
   {
      if (map == nullptr)
      {
         return std::nullopt;
      }

      const double toleranceM = MeasureHandleToleranceM(map, geo.latitude_);
      std::optional<MeasureHandleSelection> best;
      double                                bestDistanceM = toleranceM;

      model_.Read(
         [&](const std::vector<MapAnnotationObject>& objects)
         {
            for (const auto& object : objects)
            {
               const auto* measure =
                  std::get_if<MapAnnotationMeasure>(&object.payload);
               if (measure == nullptr)
               {
                  continue;
               }

               const double distanceA = CoordinateGapM(measure->a, geo);
               if (distanceA <= bestDistanceM)
               {
                  bestDistanceM = distanceA;
                  best = MeasureHandleSelection {object.id, MeasureHandle::A};
               }

               const double distanceB = CoordinateGapM(measure->b, geo);
               if (distanceB <= bestDistanceM)
               {
                  bestDistanceM = distanceB;
                  best = MeasureHandleSelection {object.id, MeasureHandle::B};
               }
            }
         });

      return best;
   }

   void UpdateMeasureHandle(MapAnnotationLayer*           self,
                            const MeasureHandleSelection& selection,
                            const common::Coordinate&     geo)
   {
      std::optional<units::length::meters<double>> updatedDistanceM;

      model_.Write(
         [&](std::vector<MapAnnotationObject>& objects)
         {
            for (auto& object : objects)
            {
               if (object.id != selection.id)
               {
                  continue;
               }

               auto* measure =
                  std::get_if<MapAnnotationMeasure>(&object.payload);
               if (measure == nullptr)
               {
                  continue;
               }

               if (selection.handle == MeasureHandle::A)
               {
                  measure->a = geo;
               }
               else
               {
                  measure->b = geo;
               }

               updatedDistanceM = MeasureDistanceM(*measure);
               break;
            }
         });

      if (!updatedDistanceM.has_value())
      {
         return;
      }

      draw_->Rebuild();
      EmitMeasureUpdated(self, *updatedDistanceM);
   }

   void FlushPendingEraseGpuUpdate(MapAnnotationLayer* self)
   {
      if (!eraseGpuDirty_)
      {
         return;
      }

      draw_->RemoveCommittedObjects(pendingGpuEraseIds_);
      pendingGpuEraseIds_.clear();
      eraseGpuDirty_ = false;
      Q_EMIT self->NeedsRendering();
      if (mapContext_ != nullptr)
      {
         if (QWidget* const widget = mapContext_->widget(); widget != nullptr)
         {
            widget->update();
         }
      }
   }

   void EraseAlongPixelSegment(MapAnnotationLayer*                    self,
                               const std::shared_ptr<QMapLibre::Map>& map,
                               const QPointF&                         fromPx,
                               const QPointF&                         toPx)
   {
      if (map == nullptr)
      {
         return;
      }

      const units::length::meters<double> eraserHalfM {
         self->style().strokeWidthM * 0.5};

      const units::length::meters<double> mpp =
         util::maplibre::MetersPerPixelAt(map, fromPx);
      const double eraseHalfM = eraserHalfM.value();
      const double mppValue   = mpp.value();
      const double eraseRadiusPx =
         (mppValue > 0.0) ? (eraseHalfM / mppValue) : 8.0;
      constexpr double kMinSampleSpacingPx {2.0};
      const double     brushStepPx =
         std::max(kMinSampleSpacingPx, eraseRadiusPx * 0.5);
      const double sampleSpacingPx = std::max(kMinSampleSpacingPx, brushStepPx);

      const double dx  = toPx.x() - fromPx.x();
      const double dy  = toPx.y() - fromPx.y();
      const double len = std::hypot(dx, dy);
      const int    steps =
         std::max(1, static_cast<int>(std::ceil(len / sampleSpacingPx)));

      std::unordered_set<std::uint64_t> removeIds;

      for (int i = 0; i <= steps; ++i)
      {
         const double  t = static_cast<double>(i) / static_cast<double>(steps);
         const QPointF px(fromPx.x() + dx * t, fromPx.y() + dy * t);
         const auto    c = map->coordinateForPixel(px);
         const common::Coordinate geo {c.first, c.second};
         for (const auto id : draw_->PickObjects(geo, eraserHalfM))
         {
            if (erasedIds_.insert(id).second)
            {
               removeIds.insert(id);
            }
         }
      }

      if (removeIds.empty())
      {
         return;
      }

      model_.Write(
         [&removeIds](std::vector<MapAnnotationObject>& objects)
         {
            std::erase_if(objects,
                          [&removeIds](const MapAnnotationObject& object)
                          { return removeIds.contains(object.id); });
         });
      pendingGpuEraseIds_.insert(removeIds.begin(), removeIds.end());
      eraseGpuDirty_ = true;
   }
};

MapAnnotationLayer::MapAnnotationLayer(
   std::shared_ptr<render::RenderContext> renderContext) :
    GenericLayer(renderContext), p(std::make_unique<Impl>(std::move(renderContext)))
{
}
MapAnnotationLayer::~MapAnnotationLayer() = default;

void MapAnnotationLayer::Initialize(
   const std::shared_ptr<MapContext>& mapContext)
{
   p->mapContext_ = mapContext;
#if !defined(SCWX_RENDER_BACKEND_VULKAN)
   p->draw_->Initialize();
#else
   p->draw_->Rebuild();
#endif
}

void MapAnnotationLayer::Deinitialize()
{
#if !defined(SCWX_RENDER_BACKEND_VULKAN)
   p->draw_->Deinitialize();
#endif
}

void MapAnnotationLayer::Render(
   const std::shared_ptr<MapContext>& /* mapContext */,
   const QMapLibre::CustomLayerRenderParameters& /* params */)
{
}

#if defined(SCWX_RENDER_BACKEND_VULKAN)
void MapAnnotationLayer::RenderVulkanOverlay(
   QRhiCommandBuffer*                            commandBuffer,
   render::RhiVulkanOverlayResources&            resources,
   const std::shared_ptr<MapContext>& /* mapContext */,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   if (!p->visible_)
   {
      return;
   }

   p->draw_->RenderVulkan(commandBuffer, resources, params, false);
}
#endif

bool MapAnnotationLayer::RunMousePicking(
   const std::shared_ptr<MapContext>& /* mapContext */,
   const QMapLibre::CustomLayerRenderParameters& /* params */,
   const QPointF& /* mouseLocalPos */,
   const QPointF& /* mouseGlobalPos */,
   const glm::vec2& /* mouseCoords */,
   const common::Coordinate& /* mouseGeoCoords */,
   std::shared_ptr<types::EventHandler>& /* eventHandler */)
{
   if (!p->visible_)
   {
      return false;
   }
   return false;
}

bool MapAnnotationLayer::ConsumesLeftDrag() const
{
   return p->visible_ && p->tool_ != MapAnnotationTool::None;
}

bool MapAnnotationLayer::IsActivelyDrawing() const
{
   return p->drawing_;
}

void MapAnnotationLayer::SetTool(MapAnnotationTool tool)
{
   p->tool_ = tool;
   p->measureA_.reset();
   p->CancelInteraction();
   Q_EMIT NeedsRendering();
   Q_EMIT ToolChanged(tool);
}

MapAnnotationTool MapAnnotationLayer::tool() const
{
   return p->tool_;
}

void MapAnnotationLayer::SetStyle(const MapAnnotationStyle& style)
{
   p->style_ = style;
   if (p->drawing_)
   {
      p->UpdatePreview();
   }
   Q_EMIT NeedsRendering();
}

MapAnnotationStyle MapAnnotationLayer::style() const
{
   return p->style_;
}

void MapAnnotationLayer::SetVisible(bool visible)
{
   if (p->visible_ == visible)
   {
      return;
   }
   p->visible_ = visible;
   if (!visible)
   {
      p->CancelInteraction();
   }
   Q_EMIT NeedsRendering();
}

bool MapAnnotationLayer::visible() const
{
   return p->visible_;
}

void MapAnnotationLayer::CancelInteraction()
{
   p->CancelInteraction();
   Q_EMIT NeedsRendering();
}

void MapAnnotationLayer::ClearAll()
{
   p->model_.Clear();
   p->measureA_.reset();
   p->lastMeasureM_.reset();
   p->CancelInteraction();
   p->draw_->Rebuild();
   Q_EMIT NeedsRendering();
}

std::size_t MapAnnotationLayer::GetObjectCount() const
{
   std::size_t count {0};
   p->model_.Read([&count](const std::vector<MapAnnotationObject>& objects)
                  { count = objects.size(); });
   return count;
}

std::optional<units::length::meters<double>>
MapAnnotationLayer::LastMeasureDistanceM() const
{
   return p->lastMeasureM_;
}

std::vector<MapAnnotationLayer::MeasurementOverlay>
MapAnnotationLayer::GetMeasurementOverlays() const
{
   std::vector<MeasurementOverlay> overlays;

   if (!p->visible_)
   {
      return overlays;
   }

   p->model_.Read(
      [&](const std::vector<MapAnnotationObject>& objects)
      {
         overlays.reserve(objects.size());
         for (const auto& object : objects)
         {
            const auto* measure =
               std::get_if<MapAnnotationMeasure>(&object.payload);
            if (measure == nullptr)
            {
               continue;
            }

            overlays.push_back(MeasurementOverlay {
               .id = object.id,
               .a  = measure->a,
               .b  = measure->b,
               .labelAnchor =
                  common::Coordinate {
                     (measure->a.latitude_ + measure->b.latitude_) *
                        kMeasurementAnchorRatio,
                     (measure->a.longitude_ + measure->b.longitude_) *
                        kMeasurementAnchorRatio},
               .distanceM = MeasureDistanceM(*measure),
            });
         }
      });

   return overlays;
}

void MapAnnotationLayer::HandleMousePress(
   const std::shared_ptr<QMapLibre::Map>& map, const QPointF& localPos)
{
   if (map == nullptr || !p->visible_)
   {
      return;
   }

   const auto c = map->coordinateForPixel(localPos);

   if (p->tool_ == MapAnnotationTool::Erase)
   {
      p->drawing_        = true;
      p->lastErasePixel_ = localPos;
      p->erasedIds_.clear();
      p->pendingGpuEraseIds_.clear();
      p->EraseAlongPixelSegment(this, map, localPos, localPos);
      p->FlushPendingEraseGpuUpdate(this);
      return;
   }

   if (p->tool_ == MapAnnotationTool::None)
   {
      return;
   }

   p->pressGeo_ = {c.first, c.second};

   switch (p->tool_)
   {
   case MapAnnotationTool::Measure:
      if (auto selection = p->FindMeasureHandle(map, p->pressGeo_);
          selection.has_value())
      {
         const auto activeSelection = *selection;
         p->draggedMeasureHandle_   = activeSelection;
         p->drawing_                = true;
         p->UpdateMeasureHandle(this, activeSelection, p->pressGeo_);
      }
      else if (!p->measureA_.has_value())
      {
         p->measureA_    = p->pressGeo_;
         p->draftPoints_ = {p->pressGeo_};
         p->UpdatePreview();
      }
      else if (const auto measureA = p->measureA_)
      {
         MapAnnotationMeasure m;
         m.a = *measureA;
         m.b = p->pressGeo_;
         MapAnnotationObject obj {};
         obj.payload = m;
         obj.style   = p->style_;
         static_cast<void>(p->model_.Add(std::move(obj)));
         p->EmitMeasureUpdated(this, MeasureDistanceM(m));
         p->measureA_.reset();
         p->draftPoints_.clear();
         p->draw_->ClearPreview();
         p->draw_->Rebuild();
      }
      Q_EMIT NeedsRendering();
      return;

   default:
      break;
   }

   p->drawing_ = true;

   switch (p->tool_)
   {
   case MapAnnotationTool::Freehand:
      p->draftPoints_.clear();
      p->draftPoints_.push_back(p->pressGeo_);
      p->lastFreehandPixel_ = localPos;
      p->UpdatePreview();
      break;

   case MapAnnotationTool::Circle:
      p->circleCenter_ = p->pressGeo_;
      break;

   case MapAnnotationTool::Rectangle:
      p->rectCorner_ = p->pressGeo_;
      break;

   default:
      break;
   }

   Q_EMIT NeedsRendering();
}

void MapAnnotationLayer::HandleMouseMove(
   const std::shared_ptr<QMapLibre::Map>& map, const QPointF& localPos)
{
   if (!p->visible_ || !p->drawing_ || map == nullptr)
   {
      return;
   }

   if (p->tool_ == MapAnnotationTool::Freehand)
   {
      if (p->draftPoints_.empty())
      {
         return;
      }
      if (const auto lastFreehandPixel = p->lastFreehandPixel_)
      {
         AppendFreehandAlongPixelSegment(
            p->draftPoints_, map, *lastFreehandPixel, localPos);
         p->lastFreehandPixel_ = localPos;
         constexpr std::size_t kLiveSimplifyPointInterval {48};
         if (p->draftPoints_.size() >= kLiveSimplifyPointInterval)
         {
            const double toleranceM =
               std::clamp(p->style_.strokeWidthM.value() * 0.04, 2.0, 30.0);
            SimplifyFreehandPoints(p->draftPoints_, toleranceM);
         }
         p->UpdatePreview();
         Q_EMIT NeedsRendering();
      }
      return;
   }

   if (p->tool_ == MapAnnotationTool::Erase)
   {
      const QPointF fromPx = p->lastErasePixel_.value_or(localPos);
      p->EraseAlongPixelSegment(this, map, fromPx, localPos);
      p->lastErasePixel_ = localPos;
      p->FlushPendingEraseGpuUpdate(this);
      return;
   }

   if (p->tool_ == MapAnnotationTool::Measure)
   {
      if (auto draggedMeasureHandle = p->draggedMeasureHandle_)
      {
         const auto               c = map->coordinateForPixel(localPos);
         const common::Coordinate geo {c.first, c.second};
         p->UpdateMeasureHandle(this, *draggedMeasureHandle, geo);
         Q_EMIT NeedsRendering();
      }
      return;
   }

   const auto               c = map->coordinateForPixel(localPos);
   const common::Coordinate geo {c.first, c.second};

   if (p->tool_ == MapAnnotationTool::Line)
   {
      p->draftPoints_.clear();
      p->draftPoints_.push_back(p->pressGeo_);
      p->draftPoints_.push_back(geo);
      p->UpdatePreview();
      Q_EMIT NeedsRendering();
      return;
   }

   if (p->tool_ == MapAnnotationTool::Circle)
   {
      if (auto circleCenter = p->circleCenter_)
      {
         p->draftPoints_.clear();
         p->draftPoints_.push_back(*circleCenter);
         p->draftPoints_.push_back(geo);
         p->UpdatePreview();
         Q_EMIT NeedsRendering();
      }
      return;
   }

   if (p->tool_ == MapAnnotationTool::Rectangle)
   {
      if (auto rectCorner = p->rectCorner_)
      {
         p->draftPoints_.clear();
         p->draftPoints_.push_back(*rectCorner);
         p->draftPoints_.push_back(geo);
         p->UpdatePreview();
         Q_EMIT NeedsRendering();
      }
   }
}

void MapAnnotationLayer::HandleMouseRelease(
   const std::shared_ptr<QMapLibre::Map>& map, const QPointF& localPos)
{
   if (map == nullptr || !p->visible_)
   {
      return;
   }

   const auto               c = map->coordinateForPixel(localPos);
   const common::Coordinate geo {c.first, c.second};

   if (p->tool_ == MapAnnotationTool::Erase)
   {
      p->FlushPendingEraseGpuUpdate(this);
      p->drawing_ = false;
      p->lastErasePixel_.reset();
      p->erasedIds_.clear();
      return;
   }

   if (p->tool_ == MapAnnotationTool::Measure)
   {
      p->drawing_ = false;
      p->draggedMeasureHandle_.reset();
      return;
   }

   if (p->tool_ == MapAnnotationTool::Line)
   {
      if (p->drawing_)
      {
         p->draftPoints_ = {p->pressGeo_, geo};
         p->CommitPolyline(false);
      }
      p->drawing_ = false;
      p->draw_->ClearPreview();
      Q_EMIT NeedsRendering();
      return;
   }

   if (p->tool_ == MapAnnotationTool::Circle)
   {
      if (auto circleCenter = p->circleCenter_)
      {
         const double r =
            util::GeographicLib::GetDistance(circleCenter->latitude_,
                                             circleCenter->longitude_,
                                             geo.latitude_,
                                             geo.longitude_)
               .value();
         if (r > kMinimumCircleRadiusM)
         {
            MapAnnotationCircle circle;
            circle.center       = *circleCenter;
            circle.radiusMeters = r;
            MapAnnotationObject obj {};
            obj.payload = circle;
            obj.style   = p->style_;
            static_cast<void>(p->model_.Add(std::move(obj)));
            p->draw_->Rebuild();
         }
      }
      p->circleCenter_.reset();
      p->drawing_ = false;
      p->draw_->ClearPreview();
      Q_EMIT NeedsRendering();
      return;
   }

   if (p->tool_ == MapAnnotationTool::Rectangle)
   {
      if (auto rectCorner = p->rectCorner_)
      {
         MapAnnotationRectangle rect;
         rect.corner1   = *rectCorner;
         rect.corner2   = geo;
         rect.fill      = p->style_.polygonFill;
         rect.hatchFill = p->style_.hatchFill;
         MapAnnotationObject obj {};
         obj.payload = rect;
         obj.style   = p->style_;
         static_cast<void>(p->model_.Add(std::move(obj)));
         p->draw_->Rebuild();
      }
      p->rectCorner_.reset();
      p->drawing_ = false;
      p->draw_->ClearPreview();
      Q_EMIT NeedsRendering();
      return;
   }

   if (p->tool_ == MapAnnotationTool::Freehand)
   {
      if (!p->draftPoints_.empty())
      {
         p->CommitPolyline(true);
      }
      p->drawing_ = false;
      p->lastFreehandPixel_.reset();
      Q_EMIT NeedsRendering();
   }
}

} // namespace scwx::qt::map
