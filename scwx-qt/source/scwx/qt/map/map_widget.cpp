#include "map_widget.hpp"

#include <scwx/qt/map/triangle_layer.hpp>

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

typedef std::pair<std::string, std::string> MapStyle;

// clang-format off
static const MapStyle streets          { "mapbox://styles/mapbox/streets-v11",           "Streets"};
static const MapStyle outdoors         { "mapbox://styles/mapbox/outdoors-v11",          "Outdoors"};
static const MapStyle light            { "mapbox://styles/mapbox/light-v10",             "Light"};
static const MapStyle dark             { "mapbox://styles/mapbox/dark-v10",              "Dark" };
static const MapStyle satellite        { "mapbox://styles/mapbox/satellite-v9",          "Satellite" };
static const MapStyle satelliteStreets { "mapbox://styles/mapbox/satellite-streets-v11", "Satellite Streets" };
// clang-format on

static const std::array<MapStyle, 6> mapboxStyles_ = {
   {streets, outdoors, light, dark, satellite, satelliteStreets}};

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
   static uint8_t currentStyleIndex = 0;

   auto& styles = mapboxStyles_;

   map_->setStyleUrl(styles[currentStyleIndex].first.c_str());
   setWindowTitle(QString("Mapbox GL: ") +
                  styles[currentStyleIndex].second.c_str());

   if (++currentStyleIndex == styles.size())
   {
      currentStyleIndex = 0;
   }
}
void MapWidget::AddLayers()
{
   // QMapboxGL::addCustomLayer will take ownership of the QScopedPointer
   QScopedPointer<QMapbox::CustomLayerHostInterface> pHost(new TriangleLayer());

   QString before = "ferry";

   for (const QString& layer : map_->layerIds())
   {
      // Draw below tunnels, ferries and roads
      if (layer.startsWith("tunnel") || layer.startsWith("ferry") ||
          layer.startsWith("road"))
      {
         before = layer;
         break;
      }
   }

   map_->addCustomLayer("triangle", pHost, before);
}

void MapWidget::keyPressEvent(QKeyEvent* ev)
{
   switch (ev->key())
   {
   case Qt::Key_S: changeStyle(); break;
   case Qt::Key_L:
   {
      for (const QString& layer : map_->layerIds())
      {
         qDebug() << "Layer: " << layer;
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

   connect(map_.get(), &QMapboxGL::mapChanged, this, &MapWidget::mapChanged);
}

void MapWidget::paintGL()
{
   frameDraws_++;
   map_->resize(size());
   map_->setFramebufferObject(defaultFramebufferObject(),
                              size() * pixelRatio());
   map_->render();
}

void MapWidget::mapChanged(QMapboxGL::MapChange mapChange)
{
   if (mapChange == QMapboxGL::MapChangeDidFinishLoadingStyle)
   {
      AddLayers();
   }
}

} // namespace qt
} // namespace scwx
