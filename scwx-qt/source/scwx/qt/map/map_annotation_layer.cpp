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
#include <glm/gtc/type_ptr.hpp>

namespace scwx::qt::map
{

namespace
{
enum class MeasureHandle
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

double MeasureDistanceM(const MapAnnotationMeasure& measure)
{
   return util::GeographicLib::GetDistance(measure.a.latitude_,
                                           measure.a.longitude_,
                                           measure.b.latitude_,
                                           measure.b.longitude_)
      .value();
}
} // namespace

class MapAnnotationLayer::Impl
{
public:
   explicit Impl(std::shared_ptr<gl::GlContext> glContext) :
       glContext_ {std::move(glContext)},
       model_ {},
       draw_ {std::make_shared<gl::draw::MapAnnotationsDrawItem>(glContext_,
                                                                 &model_)}
   {
   }

   std::shared_ptr<MapContext>                       mapContext_;
   std::shared_ptr<gl::GlContext>                    glContext_;
   MapAnnotationModel                                model_;
   std::shared_ptr<gl::draw::MapAnnotationsDrawItem> draw_;

   MapAnnotationTool  tool_ {MapAnnotationTool::None};
   MapAnnotationStyle style_ {};
   bool               visible_ {true};

   bool                                  drawing_ {false};
   std::vector<common::Coordinate>       draftPoints_ {};
   common::Coordinate                    pressGeo_ {};
   std::optional<common::Coordinate>     circleCenter_ {};
   std::optional<common::Coordinate>     rectCorner_ {};
   std::optional<common::Coordinate>     measureA_ {};
   std::optional<double>                 lastMeasureM_ {};
   std::optional<MeasureHandleSelection> draggedMeasureHandle_ {};
   /** Last pointer position (widget px) while freehand drawing; drives pixel
    * resampling. */
   std::optional<QPointF>            lastFreehandPixel_ {};
   std::optional<QPointF>            lastErasePixel_ {};
   std::unordered_set<std::uint64_t> erasedIds_ {};

   void EmitMeasureUpdated(MapAnnotationLayer* self, double distanceM)
   {
      lastMeasureM_ = distanceM;
      Q_EMIT self->MeasureUpdated(distanceM);
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
      draw_->ClearPreview();
      draw_->Rebuild();
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
         QMapLibre::metersPerPixelAtLatitude(latitude, map->zoom()) * 12.0,
         style_.strokeWidthM.value() * 2.0);
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
      std::optional<double> updatedDistanceM;

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

   void EraseAlongPixelSegment(MapAnnotationLayer*                    self,
                               const std::shared_ptr<QMapLibre::Map>& map,
                               const QPointF&                         fromPx,
                               const QPointF&                         toPx)
   {
      if (map == nullptr)
      {
         return;
      }

      const double     dx  = toPx.x() - fromPx.x();
      const double     dy  = toPx.y() - fromPx.y();
      const double     len = std::hypot(dx, dy);
      constexpr double kErasePixelStep {6.0};
      const int        steps =
         std::max(1, static_cast<int>(std::ceil(len / kErasePixelStep)));

      std::unordered_set<std::uint64_t> removeIds;

      for (int i = 0; i <= steps; ++i)
      {
         const double  t = static_cast<double>(i) / static_cast<double>(steps);
         const QPointF px(fromPx.x() + dx * t, fromPx.y() + dy * t);
         const auto    c = map->coordinateForPixel(px);
         const glm::vec2 mc =
            util::maplibre::LatLongToScreenCoordinate({c.first, c.second});
         const common::Coordinate geo {c.first, c.second};
         for (const auto id : draw_->PickObjects(mc, geo))
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
      draw_->Rebuild();
      Q_EMIT self->NeedsRendering();
   }
};

MapAnnotationLayer::MapAnnotationLayer(
   std::shared_ptr<gl::GlContext> glContext) :
    GenericLayer(std::move(glContext)), p(std::make_unique<Impl>(gl_context()))
{
}
MapAnnotationLayer::~MapAnnotationLayer() = default;

void MapAnnotationLayer::Initialize(
   const std::shared_ptr<MapContext>& mapContext)
{
   p->mapContext_ = mapContext;
   p->draw_->Initialize();
}

void MapAnnotationLayer::Deinitialize()
{
   p->draw_->Deinitialize();
}

void MapAnnotationLayer::Render(
   const std::shared_ptr<MapContext>& /* mapContext */,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   if (!p->visible_)
   {
      return;
   }
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   p->draw_->Render(params);
}

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

void MapAnnotationLayer::SetTool(MapAnnotationTool tool)
{
   p->tool_ = tool;
   p->measureA_.reset();
   p->CancelInteraction();
   Q_EMIT ToolChanged(tool);
}

MapAnnotationTool MapAnnotationLayer::tool() const
{
   return p->tool_;
}

void MapAnnotationLayer::SetStyle(const MapAnnotationStyle& style)
{
   p->style_ = style;
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
   p->CancelInteraction();
   p->draw_->Rebuild();
}

std::optional<double> MapAnnotationLayer::LastMeasureDistanceM() const
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
                     (measure->a.latitude_ + measure->b.latitude_) * 0.5,
                     (measure->a.longitude_ + measure->b.longitude_) * 0.5},
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
      p->EraseAlongPixelSegment(this, map, localPos, localPos);
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
         p->draggedMeasureHandle_ = *selection;
         p->drawing_              = true;
         p->UpdateMeasureHandle(this, *selection, p->pressGeo_);
      }
      else if (!p->measureA_.has_value())
      {
         p->measureA_    = p->pressGeo_;
         p->draftPoints_ = {p->pressGeo_};
         p->UpdatePreview();
      }
      else
      {
         MapAnnotationMeasure m;
         m.a = *p->measureA_;
         m.b = p->pressGeo_;
         MapAnnotationObject obj {};
         obj.payload = m;
         obj.style   = p->style_;
         static_cast<void>(p->model_.Add(std::move(obj)));
         const double d = MeasureDistanceM(m);
         p->EmitMeasureUpdated(this, d);
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

   const auto               c = map->coordinateForPixel(localPos);
   const common::Coordinate geo {c.first, c.second};

   if (p->tool_ == MapAnnotationTool::Freehand)
   {
      if (p->draftPoints_.empty() || !p->lastFreehandPixel_.has_value())
      {
         return;
      }
      AppendFreehandAlongPixelSegment(
         p->draftPoints_, map, *p->lastFreehandPixel_, localPos);
      p->lastFreehandPixel_ = localPos;
      p->UpdatePreview();
      Q_EMIT NeedsRendering();
      return;
   }

   if (p->tool_ == MapAnnotationTool::Erase)
   {
      if (!p->lastErasePixel_.has_value())
      {
         p->lastErasePixel_ = localPos;
      }
      p->EraseAlongPixelSegment(this, map, *p->lastErasePixel_, localPos);
      p->lastErasePixel_ = localPos;
      return;
   }

   if (p->tool_ == MapAnnotationTool::Measure)
   {
      if (p->draggedMeasureHandle_.has_value())
      {
         p->UpdateMeasureHandle(this, *p->draggedMeasureHandle_, geo);
         Q_EMIT NeedsRendering();
      }
      return;
   }

   if (p->tool_ == MapAnnotationTool::Line)
   {
      p->draftPoints_.clear();
      p->draftPoints_.push_back(p->pressGeo_);
      p->draftPoints_.push_back(geo);
      p->UpdatePreview();
      Q_EMIT NeedsRendering();
      return;
   }

   if (p->tool_ == MapAnnotationTool::Circle && p->circleCenter_.has_value())
   {
      p->draftPoints_.clear();
      p->draftPoints_.push_back(*p->circleCenter_);
      p->draftPoints_.push_back(geo);
      p->UpdatePreview();
      Q_EMIT NeedsRendering();
      return;
   }

   if (p->tool_ == MapAnnotationTool::Rectangle && p->rectCorner_.has_value())
   {
      p->draftPoints_.clear();
      p->draftPoints_.push_back(*p->rectCorner_);
      p->draftPoints_.push_back(geo);
      p->UpdatePreview();
      Q_EMIT NeedsRendering();
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

   if (p->tool_ == MapAnnotationTool::Circle && p->circleCenter_.has_value())
   {
      const double r =
         util::GeographicLib::GetDistance(p->circleCenter_->latitude_,
                                          p->circleCenter_->longitude_,
                                          geo.latitude_,
                                          geo.longitude_)
            .value();
      if (r > 10.0)
      {
         MapAnnotationCircle circle;
         circle.center       = *p->circleCenter_;
         circle.radiusMeters = r;
         MapAnnotationObject obj {};
         obj.payload = std::move(circle);
         obj.style   = p->style_;
         static_cast<void>(p->model_.Add(std::move(obj)));
         p->draw_->Rebuild();
      }
      p->circleCenter_.reset();
      p->drawing_ = false;
      p->draw_->ClearPreview();
      Q_EMIT NeedsRendering();
      return;
   }

   if (p->tool_ == MapAnnotationTool::Rectangle && p->rectCorner_.has_value())
   {
      MapAnnotationRectangle rect;
      rect.corner1   = *p->rectCorner_;
      rect.corner2   = geo;
      rect.fill      = p->style_.polygonFill;
      rect.hatchFill = p->style_.hatchFill;
      MapAnnotationObject obj {};
      obj.payload = std::move(rect);
      obj.style   = p->style_;
      static_cast<void>(p->model_.Add(std::move(obj)));
      p->rectCorner_.reset();
      p->drawing_ = false;
      p->draw_->Rebuild();
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
      p->draw_->ClearPreview();
      Q_EMIT NeedsRendering();
   }
}

} // namespace scwx::qt::map
