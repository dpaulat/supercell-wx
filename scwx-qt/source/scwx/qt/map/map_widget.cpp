#include "map_widget.hpp"

#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QFile>
#include <QIcon>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QString>

namespace scwx
{
namespace qt
{

MapWidget::MapWidget(const QMapboxGLSettings& settings) : settings_(settings)
{
   setFocusPolicy(Qt::StrongFocus);
}

MapWidget::~MapWidget()
{
   // Make sure we have a valid context so we
   // can delete the QMapboxGL.
   makeCurrent();
}

qreal MapWidget::pixelRatio()
{
   return devicePixelRatioF();
}

void MapWidget::changeStyle()
{
   static uint8_t currentStyleIndex;

   auto& styles = QMapbox::defaultStyles();

   map_->setStyleUrl(styles[currentStyleIndex].first);
   setWindowTitle(QString("Mapbox GL: ") + styles[currentStyleIndex].second);

   if (++currentStyleIndex == styles.size())
   {
      currentStyleIndex = 0;
   }

   sourceAdded_ = false;
}

void MapWidget::keyPressEvent(QKeyEvent* ev)
{
   switch (ev->key())
   {
   case Qt::Key_S: changeStyle(); break;
   case Qt::Key_L:
   {
      if (sourceAdded_)
      {
         return;
      }

      sourceAdded_ = true;

      // Not in all styles, but will work on streets
      QString before = "waterway-label";

      QFile geojson(":source1.geojson");
      geojson.open(QIODevice::ReadOnly);

      // The data source for the route line and markers
      QVariantMap routeSource;
      routeSource["type"] = "geojson";
      routeSource["data"] = geojson.readAll();
      map_->addSource("routeSource", routeSource);

      // The route case, painted before the route
      QVariantMap routeCase;
      routeCase["id"]     = "routeCase";
      routeCase["type"]   = "line";
      routeCase["source"] = "routeSource";
      map_->addLayer(routeCase, before);

      map_->setPaintProperty("routeCase", "line-color", QColor("white"));
      map_->setPaintProperty("routeCase", "line-width", 20.0);
      map_->setLayoutProperty("routeCase", "line-join", "round");
      map_->setLayoutProperty("routeCase", "line-cap", "round");

      // The route, painted on top of the route case
      QVariantMap route;
      route["id"]     = "route";
      route["type"]   = "line";
      route["source"] = "routeSource";
      map_->addLayer(route, before);

      map_->setPaintProperty("route", "line-color", QColor("blue"));
      map_->setPaintProperty("route", "line-width", 8.0);
      map_->setLayoutProperty("route", "line-join", "round");
      map_->setLayoutProperty("route", "line-cap", "round");

      QVariantList lineDashArray;
      lineDashArray.append(1);
      lineDashArray.append(2);

      map_->setPaintProperty("route", "line-dasharray", lineDashArray);

      // Markers at the beginning and end of the route
      map_->addImage("label-arrow", QImage(":label-arrow.svg"));
      map_->addImage("label-background", QImage(":label-background.svg"));

      QVariantMap markerArrow;
      markerArrow["id"]     = "markerArrow";
      markerArrow["type"]   = "symbol";
      markerArrow["source"] = "routeSource";
      map_->addLayer(markerArrow);

      map_->setLayoutProperty("markerArrow", "icon-image", "label-arrow");
      map_->setLayoutProperty("markerArrow", "icon-size", 0.5);
      map_->setLayoutProperty("markerArrow", "icon-ignore-placement", true);

      QVariantList arrowOffset;
      arrowOffset.append(0.0);
      arrowOffset.append(-15.0);
      map_->setLayoutProperty("markerArrow", "icon-offset", arrowOffset);

      QVariantMap markerBackground;
      markerBackground["id"]     = "markerBackground";
      markerBackground["type"]   = "symbol";
      markerBackground["source"] = "routeSource";
      map_->addLayer(markerBackground);

      map_->setLayoutProperty(
         "markerBackground", "icon-image", "label-background");
      map_->setLayoutProperty("markerBackground", "text-field", "{name}");
      map_->setLayoutProperty("markerBackground", "icon-text-fit", "both");
      map_->setLayoutProperty(
         "markerBackground", "icon-ignore-placement", true);
      map_->setLayoutProperty(
         "markerBackground", "text-ignore-placement", true);
      map_->setLayoutProperty("markerBackground", "text-anchor", "left");
      map_->setLayoutProperty("markerBackground", "text-size", 16.0);
      map_->setLayoutProperty("markerBackground", "text-padding", 0.0);
      map_->setLayoutProperty("markerBackground", "text-line-height", 1.0);
      map_->setLayoutProperty("markerBackground", "text-max-width", 8.0);

      QVariantList iconTextFitPadding;
      iconTextFitPadding.append(15.0);
      iconTextFitPadding.append(10.0);
      iconTextFitPadding.append(15.0);
      iconTextFitPadding.append(10.0);
      map_->setLayoutProperty(
         "markerBackground", "icon-text-fit-padding", iconTextFitPadding);

      QVariantList backgroundOffset;
      backgroundOffset.append(-0.5);
      backgroundOffset.append(-1.5);
      map_->setLayoutProperty(
         "markerBackground", "text-offset", backgroundOffset);

      map_->setPaintProperty("markerBackground", "text-color", QColor("white"));

      QVariantList filterExpression;
      filterExpression.append("==");
      filterExpression.append("$type");
      filterExpression.append("Point");

      QVariantList filter;
      filter.append(filterExpression);

      map_->setFilter("markerArrow", filter);
      map_->setFilter("markerBackground", filter);

      // Tilt the labels when tilting the map and make them larger
      map_->setLayoutProperty("road-label-large", "text-size", 30.0);
      map_->setLayoutProperty(
         "road-label-large", "text-pitch-alignment", "viewport");

      map_->setLayoutProperty("road-label-medium", "text-size", 30.0);
      map_->setLayoutProperty(
         "road-label-medium", "text-pitch-alignment", "viewport");

      map_->setLayoutProperty(
         "road-label-small", "text-pitch-alignment", "viewport");
      map_->setLayoutProperty("road-label-small", "text-size", 30.0);

      // Buildings extrusion
      QVariantMap buildings;
      buildings["id"]           = "3d-buildings";
      buildings["source"]       = "composite";
      buildings["source-layer"] = "building";
      buildings["type"]         = "fill-extrusion";
      buildings["minzoom"]      = 15.0;
      map_->addLayer(buildings);

      QVariantList buildingsFilterExpression;
      buildingsFilterExpression.append("==");
      buildingsFilterExpression.append("extrude");
      buildingsFilterExpression.append("true");

      QVariantList buildingsFilter;
      buildingsFilter.append(buildingsFilterExpression);

      map_->setFilter("3d-buildings", buildingsFilterExpression);

      QString fillExtrusionColorJSON = R"JSON(
              [
                "interpolate",
                ["linear"],
                ["get", "height"],
                  0.0, "blue",
                 20.0, "royalblue",
                 40.0, "cyan",
                 60.0, "lime",
                 80.0, "yellow",
                100.0, "red"
              ]
            )JSON";

      map_->setPaintProperty(
         "3d-buildings", "fill-extrusion-color", fillExtrusionColorJSON);
      map_->setPaintProperty("3d-buildings", "fill-extrusion-opacity", .6);

      QVariantMap extrusionHeight;
      extrusionHeight["type"]     = "identity";
      extrusionHeight["property"] = "height";

      map_->setPaintProperty(
         "3d-buildings", "fill-extrusion-height", extrusionHeight);

      QVariantMap extrusionBase;
      extrusionBase["type"]     = "identity";
      extrusionBase["property"] = "min_height";

      map_->setPaintProperty(
         "3d-buildings", "fill-extrusion-base", extrusionBase);
   }
   break;
   case Qt::Key_1:
   {
      if (symbolAnnotationId_.isNull())
      {
         QMapbox::Coordinate       coordinate = map_->coordinate();
         QMapbox::SymbolAnnotation symbol {coordinate, "default_marker"};
         map_->addAnnotationIcon("default_marker",
                                 QImage(":default_marker.svg"));
         symbolAnnotationId_ = map_->addAnnotation(
            QVariant::fromValue<QMapbox::SymbolAnnotation>(symbol));
      }
      else
      {
         map_->removeAnnotation(symbolAnnotationId_.toUInt());
         symbolAnnotationId_.clear();
      }
   }
   break;
   case Qt::Key_2:
   {
      if (lineAnnotationId_.isNull())
      {
         QMapbox::Coordinates coordinates;
         coordinates.push_back(map_->coordinateForPixel({0, 0}));
         coordinates.push_back(map_->coordinateForPixel(
            {qreal(size().width()), qreal(size().height())}));

         QMapbox::CoordinatesCollection collection;
         collection.push_back(coordinates);

         QMapbox::CoordinatesCollections lineGeometry;
         lineGeometry.push_back(collection);

         QMapbox::ShapeAnnotationGeometry annotationGeometry(
            QMapbox::ShapeAnnotationGeometry::LineStringType, lineGeometry);

         QMapbox::LineAnnotation line;
         line.geometry     = annotationGeometry;
         line.opacity      = 0.5f;
         line.width        = 1.0f;
         line.color        = Qt::red;
         lineAnnotationId_ = map_->addAnnotation(
            QVariant::fromValue<QMapbox::LineAnnotation>(line));
      }
      else
      {
         map_->removeAnnotation(lineAnnotationId_.toUInt());
         lineAnnotationId_.clear();
      }
   }
   break;
   case Qt::Key_3:
   {
      if (fillAnnotationId_.isNull())
      {
         QMapbox::Coordinates coordinates;
         coordinates.push_back(
            map_->coordinateForPixel({qreal(size().width()), 0}));
         coordinates.push_back(map_->coordinateForPixel(
            {qreal(size().width()), qreal(size().height())}));
         coordinates.push_back(
            map_->coordinateForPixel({0, qreal(size().height())}));
         coordinates.push_back(map_->coordinateForPixel({0, 0}));

         QMapbox::CoordinatesCollection collection;
         collection.push_back(coordinates);

         QMapbox::CoordinatesCollections fillGeometry;
         fillGeometry.push_back(collection);

         QMapbox::ShapeAnnotationGeometry annotationGeometry(
            QMapbox::ShapeAnnotationGeometry::PolygonType, fillGeometry);

         QMapbox::FillAnnotation fill;
         fill.geometry     = annotationGeometry;
         fill.opacity      = 0.5f;
         fill.color        = Qt::green;
         fill.outlineColor = QVariant::fromValue<QColor>(QColor(Qt::black));
         fillAnnotationId_ = map_->addAnnotation(
            QVariant::fromValue<QMapbox::FillAnnotation>(fill));
      }
      else
      {
         map_->removeAnnotation(fillAnnotationId_.toUInt());
         fillAnnotationId_.clear();
      }
   }
   break;
   case Qt::Key_5:
   {
      if (map_->layerExists("circleLayer"))
      {
         map_->removeLayer("circleLayer");
         map_->removeSource("circleSource");
      }
      else
      {
         QMapbox::Coordinates coordinates;
         coordinates.push_back(map_->coordinate());

         QMapbox::CoordinatesCollection collection;
         collection.push_back(coordinates);

         QMapbox::CoordinatesCollections point;
         point.push_back(collection);

         QMapbox::Feature feature(QMapbox::Feature::PointType, point, {}, {});

         QVariantMap circleSource;
         circleSource["type"] = "geojson";
         circleSource["data"] = QVariant::fromValue<QMapbox::Feature>(feature);
         map_->addSource("circleSource", circleSource);

         QVariantMap circle;
         circle["id"]     = "circleLayer";
         circle["type"]   = "circle";
         circle["source"] = "circleSource";
         map_->addLayer(circle);

         map_->setPaintProperty("circleLayer", "circle-radius", 10.0);
         map_->setPaintProperty("circleLayer", "circle-color", QColor("black"));
      }
   }
   break;
   case Qt::Key_6:
   {
      if (map_->layerExists("innerCirclesLayer") ||
          map_->layerExists("outerCirclesLayer"))
      {
         map_->removeLayer("innerCirclesLayer");
         map_->removeLayer("outerCirclesLayer");
         map_->removeSource("innerCirclesSource");
         map_->removeSource("outerCirclesSource");
      }
      else
      {
         auto makePoint = [&](double dx, double dy, const QString& color) {
            auto coordinate = map_->coordinate();
            coordinate.first += dx;
            coordinate.second += dy;
            return QMapbox::Feature {QMapbox::Feature::PointType,
                                     {{{coordinate}}},
                                     {{"color", color}},
                                     {}};
         };

         // multiple features by QVector<QMapbox::Feature>
         QVector<QMapbox::Feature> inner {makePoint(0.001, 0, "red"),
                                          makePoint(0, 0.001, "green"),
                                          makePoint(0, -0.001, "blue")};

         map_->addSource(
            "innerCirclesSource",
            {{"type", "geojson"}, {"data", QVariant::fromValue(inner)}});
         map_->addLayer({{"id", "innerCirclesLayer"},
                         {"type", "circle"},
                         {"source", "innerCirclesSource"}});

         // multiple features by QList<QMapbox::Feature>
         QList<QMapbox::Feature> outer {makePoint(0.002, 0.002, "cyan"),
                                        makePoint(-0.002, 0.002, "magenta"),
                                        makePoint(0.002, -0.002, "yellow"),
                                        makePoint(-0.002, -0.002, "black")};

         map_->addSource(
            "outerCirclesSource",
            {{"type", "geojson"}, {"data", QVariant::fromValue(outer)}});
         map_->addLayer({{"id", "outerCirclesLayer"},
                         {"type", "circle"},
                         {"source", "outerCirclesSource"}});

         QVariantList getColor {"get", "color"};
         map_->setPaintProperty("innerCirclesLayer", "circle-radius", 10.0);
         map_->setPaintProperty("innerCirclesLayer", "circle-color", getColor);
         map_->setPaintProperty("outerCirclesLayer", "circle-radius", 15.0);
         map_->setPaintProperty("outerCirclesLayer", "circle-color", getColor);
      }
   }
   break;
   default: break;
   }

   ev->accept();
}

void MapWidget::mousePressEvent(QMouseEvent* ev)
{
   lastPos_ = ev->position();

   if (ev->type() == QEvent::MouseButtonPress)
   {
      if (ev->buttons() == (Qt::LeftButton | Qt::RightButton))
      {
         changeStyle();
      }
   }

   if (ev->type() == QEvent::MouseButtonDblClick)
   {
      if (ev->buttons() == Qt::LeftButton)
      {
         map_->scaleBy(2.0, lastPos_);
      }
      else if (ev->buttons() == Qt::RightButton)
      {
         map_->scaleBy(0.5, lastPos_);
      }
   }

   ev->accept();
}

void MapWidget::mouseMoveEvent(QMouseEvent* ev)
{
   QPointF delta = ev->position() - lastPos_;

   if (!delta.isNull())
   {
      if (ev->buttons() == Qt::LeftButton &&
          ev->modifiers() & Qt::ShiftModifier)
      {
         map_->pitchBy(delta.y());
      }
      else if (ev->buttons() == Qt::LeftButton)
      {
         map_->moveBy(delta);
      }
      else if (ev->buttons() == Qt::RightButton)
      {
         map_->rotateBy(lastPos_, ev->position());
      }
   }

   lastPos_ = ev->position();
   ev->accept();
}

void MapWidget::wheelEvent(QWheelEvent* ev)
{
   if (ev->angleDelta().y() == 0)
   {
      return;
   }

   float factor = ev->angleDelta().y() / 1200.;
   if (ev->angleDelta().y() < 0)
   {
      factor = factor > -1 ? factor : 1 / factor;
   }

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
   map_->scaleBy(1 + factor, ev->position());
#else
   map_->scaleBy(1 + factor, ev->pos());
#endif

   ev->accept();
}

void MapWidget::initializeGL()
{
   map_.reset(new QMapboxGL(nullptr, settings_, size(), pixelRatio()));
   connect(map_.data(), SIGNAL(needsRendering()), this, SLOT(update()));

   // Set default location to KLSX.
   map_->setCoordinateZoom(QMapbox::Coordinate(38.6986, -90.6828), 14);

   QString styleUrl = qgetenv("MAPBOX_STYLE_URL");
   if (styleUrl.isEmpty())
   {
      changeStyle();
   }
   else
   {
      map_->setStyleUrl(styleUrl);
      setWindowTitle(QString("Mapbox GL: ") + styleUrl);
   }
}

void MapWidget::paintGL()
{
   frameDraws_++;
   map_->resize(size());
   map_->setFramebufferObject(defaultFramebufferObject(),
                              size() * pixelRatio());
   map_->render();
}

} // namespace qt
} // namespace scwx
