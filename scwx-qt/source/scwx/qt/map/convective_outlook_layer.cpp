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

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::convective_outlook_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static const std::string kSourceId_       = "convective-outlook-source";
static const std::string kFillLayerId_    = "convective-outlook-fill";
static const std::string kCigFillLayerId_ = "convective-outlook-cig-fill";
static const std::string kLineLayerId_    = "convective-outlook-line";

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

      for (const auto& polygon : data->polygons_)
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
   return kCigFillLayerId_;
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

   // Line width: thicker for CIG features
   QVariantList lineWidthExpr;
   lineWidthExpr << "match" << QVariant(QVariantList {} << "get" << "cig_level")
                 << 0 << 1.5f << 2.5f;

   // Line dash: varying dash patterns per CIG type, solid for regular
   QVariantList lineDashExpr;
   lineDashExpr << "match" << QVariant(QVariantList {} << "get" << "cig_level")
                << 0 << QVariant(QVariantList {} << 0.0 << 0.0) << 1
                << QVariant(QVariantList {} << 4.0 << 3.0) << 2
                << QVariant(QVariantList {} << 3.0 << 3.0) << 3
                << QVariant(QVariantList {} << 2.0 << 3.0)
                << QVariant(QVariantList {} << 3.0 << 3.0);

   // Serialize expressions to JSON strings for proper parsing by MapLibre
   QByteArray fillColorJson =
      QJsonDocument::fromVariant(QVariant(fillColorExpr))
         .toJson(QJsonDocument::Compact);
   QByteArray fillOpacityJson =
      QJsonDocument::fromVariant(QVariant(fillOpacityExpr))
         .toJson(QJsonDocument::Compact);
   QByteArray lineColorJson =
      QJsonDocument::fromVariant(QVariant(lineColorExpr))
         .toJson(QJsonDocument::Compact);
   QByteArray lineWidthJson =
      QJsonDocument::fromVariant(QVariant(lineWidthExpr))
         .toJson(QJsonDocument::Compact);
   QByteArray lineDashJson = QJsonDocument::fromVariant(QVariant(lineDashExpr))
                                .toJson(QJsonDocument::Compact);

   // Add fill layer
   map->addLayer(
      QString::fromStdString(kFillLayerId_),
      {{"type", "fill"}, {"source", QString::fromStdString(kSourceId_)}},
      beforeStr);
   map->setPaintProperty(QString::fromStdString(kFillLayerId_),
                         "fill-color",
                         QString::fromUtf8(fillColorJson));
   map->setPaintProperty(QString::fromStdString(kFillLayerId_),
                         "fill-opacity",
                         QString::fromUtf8(fillOpacityJson));

   // Add line layer for borders
   map->addLayer(
      QString::fromStdString(kLineLayerId_),
      {{"type", "line"}, {"source", QString::fromStdString(kSourceId_)}},
      beforeStr);
   map->setPaintProperty(QString::fromStdString(kLineLayerId_),
                         "line-color",
                         QString::fromUtf8(lineColorJson));
   map->setPaintProperty(QString::fromStdString(kLineLayerId_),
                         "line-width",
                         QString::fromUtf8(lineWidthJson));
   map->setPaintProperty(QString::fromStdString(kLineLayerId_),
                         "line-dasharray",
                         QString::fromUtf8(lineDashJson));
   map->setPaintProperty(
      QString::fromStdString(kLineLayerId_), "line-opacity", opacity / 100.0);

   // Add CIG fill layer with hatch patterns
   QVariantList cigFillPatternExpr;
   cigFillPatternExpr << "match"
                      << QVariant(QVariantList {} << "get" << "cig_level") << 1
                      << "cig-hatch-1" << 2 << "cig-hatch-2" << 3
                      << "cig-hatch-3"
                      << "";
   QByteArray cigFillPatternJson =
      QJsonDocument::fromVariant(QVariant(cigFillPatternExpr))
         .toJson(QJsonDocument::Compact);

   QVariantList cigFilter;
   cigFilter << ">" << QVariant(QVariantList {} << "get" << "cig_level") << 0;

   map->addLayer(
      QString::fromStdString(kCigFillLayerId_),
      {{"type", "fill"}, {"source", QString::fromStdString(kSourceId_)}},
      beforeStr);
   map->setPaintProperty(QString::fromStdString(kCigFillLayerId_),
                         "fill-pattern",
                         QString::fromUtf8(cigFillPatternJson));
   map->setPaintProperty(QString::fromStdString(kCigFillLayerId_),
                         "fill-opacity",
                         opacity / 100.0);
   map->setFilter(QString::fromStdString(kCigFillLayerId_),
                  QVariant(cigFilter));
}

void ConvectiveOutlookLayer::Remove(std::shared_ptr<QMapLibre::Map> map)
{
   QString sourceId  = QString::fromStdString(kSourceId_);
   QString fillId    = QString::fromStdString(kFillLayerId_);
   QString cigFillId = QString::fromStdString(kCigFillLayerId_);
   QString lineId    = QString::fromStdString(kLineLayerId_);

   if (map->layerExists(cigFillId))
   {
      map->removeLayer(cigFillId);
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
   QByteArray fillOpacityJson =
      QJsonDocument::fromVariant(QVariant(fillOpacityExpr))
         .toJson(QJsonDocument::Compact);
   map->setPaintProperty(QString::fromStdString(kFillLayerId_),
                         "fill-opacity",
                         QString::fromUtf8(fillOpacityJson));

   // Update line opacity
   map->setPaintProperty(
      QString::fromStdString(kLineLayerId_), "line-opacity", opacity / 100.0);

   // Update CIG fill layer opacity
   if (map->layerExists(QString::fromStdString(kCigFillLayerId_)))
   {
      map->setPaintProperty(QString::fromStdString(kCigFillLayerId_),
                            "fill-opacity",
                            opacity / 100.0);
   }
}

} // namespace scwx::qt::map
