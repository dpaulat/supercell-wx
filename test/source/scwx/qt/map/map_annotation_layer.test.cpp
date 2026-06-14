#include <scwx/qt/map/map_annotation_layer.hpp>
#include <scwx/qt/map/map_annotation_types.hpp>

#include <units/length.h>

#include <QCoreApplication>

#include <gtest/gtest.h>
#include <qmaplibre.hpp>

namespace scwx::qt::map
{

namespace
{

std::shared_ptr<QMapLibre::Map> CreateMap()
{
   const QMapLibre::Settings settings {};
   auto                      map =
      std::make_shared<QMapLibre::Map>(nullptr, settings, QSize {256, 256});

   map->setCoordinateZoom({35.0, -97.0}, 6.0);
   QCoreApplication::processEvents();

   return map;
}

} // namespace

TEST(MapAnnotationLayerTest, CancelInteractionClearsPendingMeasureAnchor)
{
   auto map = CreateMap();

   MapAnnotationLayer layer {nullptr};
   layer.SetTool(MapAnnotationTool::Measure);

   layer.HandleMousePress(map, QPointF {48.0, 48.0});
   EXPECT_TRUE(layer.GetMeasurementOverlays().empty());

   layer.CancelInteraction();

   layer.HandleMousePress(map, QPointF {192.0, 192.0});
   EXPECT_TRUE(layer.GetMeasurementOverlays().empty());

   layer.HandleMousePress(map, QPointF {208.0, 208.0});
   EXPECT_EQ(layer.GetMeasurementOverlays().size(), 1u);
}

TEST(MapAnnotationLayerTest, HideClearsPendingMeasureAnchor)
{
   auto map = CreateMap();

   MapAnnotationLayer layer {nullptr};
   layer.SetTool(MapAnnotationTool::Measure);

   layer.HandleMousePress(map, QPointF {64.0, 64.0});
   EXPECT_TRUE(layer.GetMeasurementOverlays().empty());

   layer.SetVisible(false);
   EXPECT_TRUE(layer.GetMeasurementOverlays().empty());

   layer.SetVisible(true);
   layer.HandleMousePress(map, QPointF {200.0, 200.0});
   EXPECT_TRUE(layer.GetMeasurementOverlays().empty());

   layer.HandleMousePress(map, QPointF {216.0, 216.0});
   EXPECT_EQ(layer.GetMeasurementOverlays().size(), 1u);
}

TEST(MapAnnotationLayerTest, CompletedMeasureTracksDistanceAndVisibility)
{
   auto map = CreateMap();

   MapAnnotationLayer layer {nullptr};
   layer.SetTool(MapAnnotationTool::Measure);

   layer.HandleMousePress(map, QPointF {72.0, 72.0});
   layer.HandleMousePress(map, QPointF {184.0, 184.0});

   const auto overlays = layer.GetMeasurementOverlays();
   ASSERT_EQ(overlays.size(), 1u);
   ASSERT_TRUE(layer.LastMeasureDistanceM().has_value());
   EXPECT_GT(layer.LastMeasureDistanceM()->value(), 0.0);
   EXPECT_DOUBLE_EQ(overlays.front().distanceM.value(),
                    layer.LastMeasureDistanceM()->value());

   layer.SetVisible(false);
   EXPECT_TRUE(layer.GetMeasurementOverlays().empty());

   layer.SetVisible(true);
   EXPECT_EQ(layer.GetMeasurementOverlays().size(), 1u);
}

TEST(MapAnnotationLayerTest, EraseWideBrushRemovesLine)
{
   auto map = CreateMap();

   MapAnnotationLayer layer {nullptr};
   MapAnnotationStyle style {};
   style.strokeWidthM = units::length::meters<double> {8000.0};
   layer.SetStyle(style);

   layer.SetTool(MapAnnotationTool::Line);
   layer.HandleMousePress(map, QPointF {40.0, 40.0});
   layer.HandleMouseMove(map, QPointF {200.0, 200.0});
   layer.HandleMouseRelease(map, QPointF {200.0, 200.0});
   ASSERT_EQ(layer.GetObjectCount(), 1u);

   layer.SetTool(MapAnnotationTool::Erase);
   layer.HandleMousePress(map, QPointF {40.0, 40.0});
   layer.HandleMouseMove(map, QPointF {200.0, 200.0});
   layer.HandleMouseRelease(map, QPointF {200.0, 200.0});
   EXPECT_EQ(layer.GetObjectCount(), 0u);
}

TEST(MapAnnotationLayerTest, SetToolClearsPendingMeasureAnchor)
{
   auto map = CreateMap();

   MapAnnotationLayer layer {nullptr};
   layer.SetTool(MapAnnotationTool::Measure);

   layer.HandleMousePress(map, QPointF {80.0, 80.0});
   layer.SetTool(MapAnnotationTool::Line);
   layer.SetTool(MapAnnotationTool::Measure);

   layer.HandleMousePress(map, QPointF {176.0, 176.0});
   EXPECT_TRUE(layer.GetMeasurementOverlays().empty());

   layer.HandleMousePress(map, QPointF {208.0, 208.0});
   EXPECT_EQ(layer.GetMeasurementOverlays().size(), 1u);
}

} // namespace scwx::qt::map
