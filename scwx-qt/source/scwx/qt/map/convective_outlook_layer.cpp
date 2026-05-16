#include <scwx/qt/map/convective_outlook_layer.hpp>
#include <scwx/spc/spc_types.hpp>
#include <scwx/qt/manager/spc_outlook_manager.hpp>
#include <scwx/util/logger.hpp>

#include <QMapLibre/Map>

#include <QByteArray>
#include <QImage>
#include <QJsonDocument>
#include <QPainter>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <string>

#include <geos/geom/CoordinateSequence.h>
#include <geos/geom/Geometry.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/LinearRing.h>
#include <geos/geom/MultiPolygon.h>
#include <geos/geom/Polygon.h>
#include <geos/operation/overlayng/OverlayNG.h>

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::convective_outlook_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static const std::string kSourceId_       = "convective-outlook-source";
static const std::string kFillLayerId_      = "convective-outlook-fill";
static const std::string kLineLayerId_    = "convective-outlook-line";
static const std::string kCigHatch1LayerId_ = "convective-outlook-cig-hatch-1";
static const std::string kCigHatch2LayerId_ = "convective-outlook-cig-hatch-2";
static const std::string kCigHatch3LayerId_ = "convective-outlook-cig-hatch-3";
static const std::string kCigLine1LayerId_  = "convective-outlook-cig-line-1";
static const std::string kCigLine2LayerId_  = "convective-outlook-cig-line-2";
static const std::string kCigLine3LayerId_  = "convective-outlook-cig-line-3";

static QImage CreateHatchPattern(int type)
{
   constexpr int kSize = 32;
   constexpr int kStep = 8;
   QImage        image(kSize, kSize, QImage::Format_ARGB32_Premultiplied);
   image.fill(Qt::transparent);

   QPainter painter(&image);
   painter.setRenderHint(QPainter::Antialiasing, true);

   if (type == 1 || type == 3)
   {
      QPen forwardPen(QColor(0, 0, 0, 255), 1.5);
      if (type == 1)
      {
         forwardPen.setDashPattern({4.0, 3.0});
      }
      painter.setPen(forwardPen);

      for (int x = -kSize; x < 2 * kSize; x += kStep)
      {
         painter.drawLine(x + kSize, 0, x, kSize);
      }
   }
   if (type == 2 || type == 3)
   {
      QPen backPen(QColor(0, 0, 0, 255), 1.5);
      painter.setPen(backPen);

      for (int x = -kSize; x < 2 * kSize; x += kStep)
      {
         painter.drawLine(x, 0, x + kSize, kSize);
      }
   }

   painter.end();
   return image;
}

namespace
{

std::unique_ptr<geos::geom::CoordinateSequence>
RingToCoordSeq(const std::vector<std::pair<double, double>>& ring)
{
   auto seq = std::make_unique<geos::geom::CoordinateSequence>();
   for (const auto& pt : ring)
   {
      seq->add(geos::geom::CoordinateXY {pt.second, pt.first});
   }
   if (seq->size() > 0)
   {
      const auto& front = seq->front<geos::geom::CoordinateXY>();
      const auto& back  = seq->back<geos::geom::CoordinateXY>();
      if (front.x != back.x || front.y != back.y)
      {
         seq->add(front);
      }
   }
   return seq;
}

std::vector<std::vector<std::pair<double, double>>>
PolygonToRings(const geos::geom::Polygon* poly)
{
   std::vector<std::vector<std::pair<double, double>>> rings;

   const auto* extRing   = poly->getExteriorRing();
   const auto* extCoords = extRing->getCoordinatesRO();
   std::vector<std::pair<double, double>> outer;
   for (size_t i = 0; i < extCoords->size(); ++i)
   {
      const auto& c = extCoords->getAt<geos::geom::CoordinateXY>(i);
      outer.emplace_back(c.y, c.x);
   }
   if (outer.size() > 1 && outer.front() == outer.back())
   {
      outer.pop_back();
   }
   rings.push_back(std::move(outer));

   for (size_t h = 0; h < poly->getNumInteriorRing(); ++h)
   {
      const auto* intRing   = poly->getInteriorRingN(h);
      const auto* intCoords = intRing->getCoordinatesRO();
      std::vector<std::pair<double, double>> hole;
      for (size_t i = 0; i < intCoords->size(); ++i)
      {
         const auto& c = intCoords->getAt<geos::geom::CoordinateXY>(i);
         hole.emplace_back(c.y, c.x);
      }
      if (hole.size() > 1 && hole.front() == hole.back())
      {
         hole.pop_back();
      }
      rings.push_back(std::move(hole));
   }

   return rings;
}

std::vector<std::vector<std::vector<std::pair<double, double>>>>
GeomToRingGroups(const geos::geom::Geometry* geom)
{
   std::vector<std::vector<std::vector<std::pair<double, double>>>> result;

   if (const auto* poly = dynamic_cast<const geos::geom::Polygon*>(geom))
   {
      result.push_back(PolygonToRings(poly));
   }
   else if (const auto* multi =
               dynamic_cast<const geos::geom::MultiPolygon*>(geom))
   {
      for (size_t i = 0; i < multi->getNumGeometries(); ++i)
      {
         const auto* subPoly =
            dynamic_cast<const geos::geom::Polygon*>(multi->getGeometryN(i));
         if (subPoly != nullptr)
         {
            result.push_back(PolygonToRings(subPoly));
         }
      }
   }

   return result;
}

} // namespace

class ConvectiveOutlookLayer::Impl
{
public:
   Impl()  = default;
   ~Impl() = default;

   std::string before_ {};

   QVariantMap BuildFeatureCollection()
   {
      auto& manager = manager::SpcOutlookManager::Instance();
      auto  data    = manager.GetOutlookData();

      QVariantMap collection;
      collection["type"] = "FeatureCollection";

      QVariantList features;

      if (data == nullptr)
      {
         collection["features"] = features;
         return collection;
      }

      // Build clipped polygon set: subtract higher CIG levels from lower ones
      // so that CIG1 hatching never appears inside CIG2/CIG3 boundaries.
      std::vector<scwx::spc::OutlookPolygon> workingPolygons;
      {
         std::vector<const scwx::spc::OutlookPolygon*> byLevel[4];
         for (const auto& polygon : data->polygons_)
         {
            int level = polygon.cigLevel_;
            if (level >= 0 && level <= 3)
            {
               byLevel[level].push_back(&polygon);
            }
         }

         int cigCount = 0;
         for (int l = 1; l <= 3; ++l)
            if (!byLevel[l].empty())
               cigCount++;

         if (cigCount < 2)
         {
            workingPolygons = data->polygons_;
         }
         else
         {
            const auto* factory =
               geos::geom::GeometryFactory::getDefaultInstance();

            auto toGeos = [&](const scwx::spc::OutlookPolygon& poly)
               -> std::unique_ptr<geos::geom::Geometry>
            {
               if (poly.rings_.empty())
                  return nullptr;
               auto outerSeq = RingToCoordSeq(poly.rings_[0]);
               if (outerSeq->size() < 4)
                  return nullptr;
               auto outerRing = factory->createLinearRing(std::move(outerSeq));

               std::vector<std::unique_ptr<geos::geom::LinearRing>> holes;
               for (size_t i = 1; i < poly.rings_.size(); ++i)
               {
                  auto holeSeq = RingToCoordSeq(poly.rings_[i]);
                  if (holeSeq->size() >= 4)
                  {
                     holes.push_back(
                        factory->createLinearRing(std::move(holeSeq)));
                  }
               }

               return factory->createPolygon(std::move(outerRing),
                                             std::move(holes));
            };

            auto accumulate = [&](std::unique_ptr<geos::geom::Geometry>& accum,
                                  const scwx::spc::OutlookPolygon&       poly)
            {
               auto g = toGeos(poly);
               if (g == nullptr)
                  return;
               if (accum == nullptr)
               {
                  accum = std::move(g);
               }
               else
               {
                  accum = accum->Union(g.get());
               }
            };

            std::unique_ptr<geos::geom::Geometry> clipGeom2;
            std::unique_ptr<geos::geom::Geometry> clipGeom3;
            std::unique_ptr<geos::geom::Geometry> clipGeom23;

            for (const auto* poly : byLevel[2])
               accumulate(clipGeom2, *poly);
            for (const auto* poly : byLevel[3])
               accumulate(clipGeom3, *poly);

            if (clipGeom2 != nullptr && clipGeom3 != nullptr)
            {
               clipGeom23 = clipGeom2->Union(clipGeom3.get());
            }
            else if (clipGeom2 != nullptr)
            {
               clipGeom23 = clipGeom2->clone();
            }
            else if (clipGeom3 != nullptr)
            {
               clipGeom23 = clipGeom3->clone();
            }

            auto clipLevel =
               [&](const std::vector<const scwx::spc::OutlookPolygon*>& src,
                   const std::unique_ptr<geos::geom::Geometry>& clipGeom)
            {
               if (clipGeom == nullptr)
               {
                  for (const auto* poly : src)
                     workingPolygons.push_back(*poly);
                  return;
               }
               for (const auto* poly : src)
               {
                  auto g = toGeos(*poly);
                  if (g == nullptr)
                  {
                     workingPolygons.push_back(*poly);
                     continue;
                  }

                  auto diff = geos::operation::overlayng::OverlayNG::overlay(
                     g.get(),
                     clipGeom.get(),
                     geos::operation::overlayng::OverlayNG::DIFFERENCE);

                  if (diff == nullptr || diff->isEmpty())
                     continue;

                  auto ringGroups = GeomToRingGroups(diff.get());
                  for (auto& rings : ringGroups)
                  {
                     if (!rings.empty())
                     {
                        scwx::spc::OutlookPolygon clipped = *poly;
                        clipped.rings_                    = std::move(rings);
                        workingPolygons.push_back(std::move(clipped));
                     }
                  }
               }
            };

            clipLevel(byLevel[1], clipGeom23);
            clipLevel(byLevel[2], clipGeom3);
            for (const auto* poly : byLevel[3])
               workingPolygons.push_back(*poly);
            for (const auto* poly : byLevel[0])
               workingPolygons.push_back(*poly);
         }
      }

      for (const auto& polygon : workingPolygons)
      {
         if (polygon.rings_.empty())
         {
            continue;
         }

         QVariantMap feature;
         feature["type"] = "Feature";

         QVariantMap geometry;
         geometry["type"] = "Polygon";

         QVariantList coords;
         for (const auto& ring : polygon.rings_)
         {
            QVariantList ringCoords;
            for (const auto& pt : ring)
            {
               QVariantList coord;
               coord << pt.second << pt.first;
               ringCoords << QVariant(coord);
            }
            coords << QVariant(ringCoords);
         }
         geometry["coordinates"] = coords;
         feature["geometry"]     = geometry;

         QVariantMap props;
         props["dn"]        = polygon.dn_;
         props["cig_level"] = polygon.cigLevel_;
         if (!polygon.fillColor_.empty())
         {
            props["fill_color"] = QString::fromStdString(polygon.fillColor_);
         }
         if (!polygon.strokeColor_.empty())
         {
            props["stroke_color"] =
               QString::fromStdString(polygon.strokeColor_);
         }
         feature["properties"] = props;

         features << QVariant(feature);
      }

      collection["features"] = features;
      return collection;
   }
};

ConvectiveOutlookLayer::ConvectiveOutlookLayer() : p(std::make_unique<Impl>())
{
}

ConvectiveOutlookLayer::~ConvectiveOutlookLayer() = default;

const std::string& ConvectiveOutlookLayer::sourceId()
{
   return kSourceId_;
}

const std::string& ConvectiveOutlookLayer::fillLayerId()
{
   return kFillLayerId_;
}

const std::string& ConvectiveOutlookLayer::cigFillLayerId()
{
   return kCigHatch1LayerId_;
}

const std::string& ConvectiveOutlookLayer::lineLayerId()
{
   return kLineLayerId_;
}

void ConvectiveOutlookLayer::Add(std::shared_ptr<QMapLibre::Map> map,
                                 const std::string&              before)
{
   logger_->debug("Add()");

   p->before_ = before;

   Remove(map);

   QVariantMap fc = p->BuildFeatureCollection();

   auto featuresList = fc["features"].toList();
   if (featuresList.isEmpty())
   {
      logger_->debug("No features to add");
      return;
   }

   QString beforeStr = QString::fromStdString(before);

   // Add GeoJSON source with FeatureCollection (JSON string)
   QVariantMap sourceOpts;
   sourceOpts["type"] = "geojson";
   sourceOpts["data"] =
      QByteArray(QJsonDocument::fromVariant(fc).toJson(QJsonDocument::Compact));
   map->addSource(QString::fromStdString(kSourceId_), sourceOpts);

   auto& manager = manager::SpcOutlookManager::Instance();
   int   opacity = manager.GetOpacity();

   // Register hatch pattern images for CIG fill
   map->addImage("cig-hatch-1", CreateHatchPattern(1));
   map->addImage("cig-hatch-2", CreateHatchPattern(2));
   map->addImage("cig-hatch-3", CreateHatchPattern(3));

   // Fill color from GeoJSON property, with fallback
   // Note: wrap inner QVariantList in QVariant() to prevent flattening by
   // QList::operator<<
   QVariantList fillColorExpr;
   fillColorExpr << "coalesce"
                 << QVariant(QVariantList {} << "get" << "fill_color")
                 << "#888888";

   // Fill opacity: transparent for CIG features (hatching overlay), normal
   // for regular
   QVariantList fillOpacityExpr;
   fillOpacityExpr << "match"
                   << QVariant(QVariantList {} << "get" << "cig_level") << 0
                   << (opacity / 100.0) << 0.0;

   // Stroke color from GeoJSON property
   QVariantList lineColorExpr;
   lineColorExpr << "coalesce"
                 << QVariant(QVariantList {} << "get" << "stroke_color")
                 << "#000000";

   // Add fill layer
   map->addLayer(
      QString::fromStdString(kFillLayerId_),
      {{"type", "fill"}, {"source", QString::fromStdString(kSourceId_)}},
      beforeStr);
   map->setPaintProperty(QString::fromStdString(kFillLayerId_),
                         "fill-color",
                         QVariant(fillColorExpr));
   map->setPaintProperty(QString::fromStdString(kFillLayerId_),
                         "fill-opacity",
                         QVariant(fillOpacityExpr));

   // Add line layer for non-CIG feature borders (cig_level == 0)
   QVariantList lineFilter;
   lineFilter << "==" << QVariant(QVariantList {} << "get" << "cig_level") << 0;

   map->addLayer(
      QString::fromStdString(kLineLayerId_),
      {{"type", "line"}, {"source", QString::fromStdString(kSourceId_)}},
      beforeStr);
   map->setPaintProperty(QString::fromStdString(kLineLayerId_),
                         "line-color",
                         QVariant(lineColorExpr));
   map->setPaintProperty(
      QString::fromStdString(kLineLayerId_), "line-width", 1.5);
   map->setPaintProperty(
      QString::fromStdString(kLineLayerId_), "line-opacity", opacity / 100.0);
   map->setFilter(QString::fromStdString(kLineLayerId_), QVariant(lineFilter));

   // Per-CIG-level hatch patterns and border lines
   // Stacked 1→2→3 so higher levels visually occlude lower ones where they
   // overlap

   // ── CIG Level 1 (bottom) ──
   map->addLayer(
      QString::fromStdString(kCigHatch1LayerId_),
      {{"type", "fill"}, {"source", QString::fromStdString(kSourceId_)}},
      beforeStr);
   map->setPaintProperty(QString::fromStdString(kCigHatch1LayerId_),
                         "fill-pattern",
                         "cig-hatch-1");
   map->setPaintProperty(QString::fromStdString(kCigHatch1LayerId_),
                         "fill-opacity",
                         opacity / 100.0);
   {
      QVariantList f;
      f << "==" << QVariant(QVariantList {} << "get" << "cig_level") << 1;
      map->setFilter(QString::fromStdString(kCigHatch1LayerId_), QVariant(f));
   }

   map->addLayer(
      QString::fromStdString(kCigLine1LayerId_),
      {{"type", "line"}, {"source", QString::fromStdString(kSourceId_)}},
      beforeStr);
   map->setPaintProperty(QString::fromStdString(kCigLine1LayerId_),
                         "line-color",
                         QVariant(lineColorExpr));
   map->setPaintProperty(
      QString::fromStdString(kCigLine1LayerId_), "line-width", 2.0);
   map->setPaintProperty(QString::fromStdString(kCigLine1LayerId_),
                         "line-dasharray",
                         QVariant(QVariantList {} << 4.0 << 3.0));
   map->setPaintProperty(QString::fromStdString(kCigLine1LayerId_),
                         "line-opacity",
                         opacity / 100.0);
   {
      QVariantList f;
      f << "==" << QVariant(QVariantList {} << "get" << "cig_level") << 1;
      map->setFilter(QString::fromStdString(kCigLine1LayerId_), QVariant(f));
   }

   // ── CIG Level 2 (covers Level 1 where overlaps) ──
   map->addLayer(
      QString::fromStdString(kCigHatch2LayerId_),
      {{"type", "fill"}, {"source", QString::fromStdString(kSourceId_)}},
      beforeStr);
   map->setPaintProperty(QString::fromStdString(kCigHatch2LayerId_),
                         "fill-pattern",
                         "cig-hatch-2");
   map->setPaintProperty(QString::fromStdString(kCigHatch2LayerId_),
                         "fill-opacity",
                         opacity / 100.0);
   {
      QVariantList f;
      f << "==" << QVariant(QVariantList {} << "get" << "cig_level") << 2;
      map->setFilter(QString::fromStdString(kCigHatch2LayerId_), QVariant(f));
   }

   map->addLayer(
      QString::fromStdString(kCigLine2LayerId_),
      {{"type", "line"}, {"source", QString::fromStdString(kSourceId_)}},
      beforeStr);
   map->setPaintProperty(QString::fromStdString(kCigLine2LayerId_),
                         "line-color",
                         QVariant(lineColorExpr));
   map->setPaintProperty(
      QString::fromStdString(kCigLine2LayerId_), "line-width", 2.0);
   map->setPaintProperty(QString::fromStdString(kCigLine2LayerId_),
                         "line-dasharray",
                         QVariant(QVariantList {} << 3.0 << 3.0));
   map->setPaintProperty(QString::fromStdString(kCigLine2LayerId_),
                         "line-opacity",
                         opacity / 100.0);
   {
      QVariantList f;
      f << "==" << QVariant(QVariantList {} << "get" << "cig_level") << 2;
      map->setFilter(QString::fromStdString(kCigLine2LayerId_), QVariant(f));
   }

   // ── CIG Level 3 (covers Level 1 and 2 where overlaps) ──
   map->addLayer(
      QString::fromStdString(kCigHatch3LayerId_),
      {{"type", "fill"}, {"source", QString::fromStdString(kSourceId_)}},
      beforeStr);
   map->setPaintProperty(QString::fromStdString(kCigHatch3LayerId_),
                         "fill-pattern",
                         "cig-hatch-3");
   map->setPaintProperty(QString::fromStdString(kCigHatch3LayerId_),
                         "fill-opacity",
                         opacity / 100.0);
   {
      QVariantList f;
      f << "==" << QVariant(QVariantList {} << "get" << "cig_level") << 3;
      map->setFilter(QString::fromStdString(kCigHatch3LayerId_), QVariant(f));
   }

   map->addLayer(
      QString::fromStdString(kCigLine3LayerId_),
      {{"type", "line"}, {"source", QString::fromStdString(kSourceId_)}},
      beforeStr);
   map->setPaintProperty(QString::fromStdString(kCigLine3LayerId_),
                         "line-color",
                         QVariant(lineColorExpr));
   map->setPaintProperty(
      QString::fromStdString(kCigLine3LayerId_), "line-width", 2.0);
   map->setPaintProperty(QString::fromStdString(kCigLine3LayerId_),
                         "line-dasharray",
                         QVariant(QVariantList {} << 2.0 << 3.0));
   map->setPaintProperty(QString::fromStdString(kCigLine3LayerId_),
                         "line-opacity",
                         opacity / 100.0);
   {
      QVariantList f;
      f << "==" << QVariant(QVariantList {} << "get" << "cig_level") << 3;
      map->setFilter(QString::fromStdString(kCigLine3LayerId_), QVariant(f));
   }
}

void ConvectiveOutlookLayer::Remove(std::shared_ptr<QMapLibre::Map> map)
{
   QString sourceId  = QString::fromStdString(kSourceId_);
   QString fillId    = QString::fromStdString(kFillLayerId_);
   QString cigHatch1Id = QString::fromStdString(kCigHatch1LayerId_);
   QString cigLine1Id  = QString::fromStdString(kCigLine1LayerId_);
   QString cigHatch2Id = QString::fromStdString(kCigHatch2LayerId_);
   QString cigLine2Id  = QString::fromStdString(kCigLine2LayerId_);
   QString cigHatch3Id = QString::fromStdString(kCigHatch3LayerId_);
   QString cigLine3Id  = QString::fromStdString(kCigLine3LayerId_);
   QString lineId      = QString::fromStdString(kLineLayerId_);

   // Remove CIG layers in reverse order (topmost first)
   if (map->layerExists(cigLine3Id))
   {
      map->removeLayer(cigLine3Id);
   }
   if (map->layerExists(cigHatch3Id))
   {
      map->removeLayer(cigHatch3Id);
   }
   if (map->layerExists(cigLine2Id))
   {
      map->removeLayer(cigLine2Id);
   }
   if (map->layerExists(cigHatch2Id))
   {
      map->removeLayer(cigHatch2Id);
   }
   if (map->layerExists(cigLine1Id))
   {
      map->removeLayer(cigLine1Id);
   }
   if (map->layerExists(cigHatch1Id))
   {
      map->removeLayer(cigHatch1Id);
   }
   if (map->layerExists(lineId))
   {
      map->removeLayer(lineId);
   }
   if (map->layerExists(fillId))
   {
      map->removeLayer(fillId);
   }
   if (map->sourceExists(sourceId))
   {
      map->removeSource(sourceId);
   }

   map->removeImage("cig-hatch-1");
   map->removeImage("cig-hatch-2");
   map->removeImage("cig-hatch-3");
}

void ConvectiveOutlookLayer::Update(std::shared_ptr<QMapLibre::Map> map)
{
   if (!map->sourceExists(QString::fromStdString(kSourceId_)))
   {
      Add(map, p->before_);
      return;
   }

   QVariantMap fc = p->BuildFeatureCollection();

   auto featuresList = fc["features"].toList();
   if (featuresList.isEmpty())
   {
      Remove(map);
      return;
   }

   auto& manager = manager::SpcOutlookManager::Instance();
   int   opacity = manager.GetOpacity();

   // Update source data (JSON string)
   QVariantMap update;
   update["data"] =
      QByteArray(QJsonDocument::fromVariant(fc).toJson(QJsonDocument::Compact));
   map->updateSource(QString::fromStdString(kSourceId_), update);

   // Update fill opacity (data-driven: 0 for CIG features, normal for
   // regular)
   QVariantList fillOpacityExpr;
   fillOpacityExpr << "match"
                   << QVariant(QVariantList {} << "get" << "cig_level") << 0
                   << (opacity / 100.0) << 0.0;
   map->setPaintProperty(QString::fromStdString(kFillLayerId_),
                         "fill-opacity",
                         QVariant(fillOpacityExpr));

   // Update line opacity
   map->setPaintProperty(
      QString::fromStdString(kLineLayerId_), "line-opacity", opacity / 100.0);

   // Update CIG layer opacities
   auto updateCigLayer = [&](const std::string& layerId, double layerOpacity)
   {
      if (map->layerExists(QString::fromStdString(layerId)))
      {
         map->setPaintProperty(
            QString::fromStdString(layerId), "fill-opacity", layerOpacity);
      }
   };
   auto updateCigLine = [&](const std::string& layerId, double lineOpacity)
   {
      if (map->layerExists(QString::fromStdString(layerId)))
      {
         map->setPaintProperty(
            QString::fromStdString(layerId), "line-opacity", lineOpacity);
      }
   };

   updateCigLayer(kCigHatch1LayerId_, opacity / 100.0);
   updateCigLine(kCigLine1LayerId_, opacity / 100.0);
   updateCigLayer(kCigHatch2LayerId_, opacity / 100.0);
   updateCigLine(kCigLine2LayerId_, opacity / 100.0);
   updateCigLayer(kCigHatch3LayerId_, opacity / 100.0);
   updateCigLine(kCigLine3LayerId_, opacity / 100.0);
}

} // namespace scwx::qt::map
