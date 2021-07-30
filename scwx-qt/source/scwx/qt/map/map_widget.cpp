#include "map_widget.hpp"

#include <scwx/qt/map/radar_layer.hpp>
#include <scwx/qt/map/radar_range_layer.hpp>
#include <scwx/qt/util/gl.hpp>

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

class MapWidgetImpl
{
public:
   explicit MapWidgetImpl(const QMapboxGLSettings& settings) :
       gl_(),
       settings_(settings),
       map_(),
       radarManager_ {std::make_shared<manager::RadarManager>()},
       lastPos_(),
       frameDraws_(0)
   {
   }
   ~MapWidgetImpl() = default;

   OpenGLFunctions gl_;

   QMapboxGLSettings          settings_;
   std::shared_ptr<QMapboxGL> map_;

   std::shared_ptr<manager::RadarManager> radarManager_;

   QPointF lastPos_;

   uint64_t frameDraws_;
};

MapWidget::MapWidget(const QMapboxGLSettings& settings) :
    p(std::make_unique<MapWidgetImpl>(settings))
{
   setFocusPolicy(Qt::StrongFocus);

   p->radarManager_->Initialize();
   QString ar2vFile = qgetenv("AR2V_FILE");
   if (!ar2vFile.isEmpty())
   {
      p->radarManager_->LoadLevel2Data(ar2vFile.toUtf8().constData());
   }
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

   p->map_->setStyleUrl(styles[currentStyleIndex].first.c_str());
   setWindowTitle(QString("Mapbox GL: ") +
                  styles[currentStyleIndex].second.c_str());

   if (++currentStyleIndex == styles.size())
   {
      currentStyleIndex = 0;
   }
}

void MapWidget::AddLayers()
{
   std::shared_ptr<view::RadarView> radarView =
      std::make_shared<view::RadarView>(p->radarManager_, p->map_);

   radarView->Initialize();

   QString colorTableFile = qgetenv("COLOR_TABLE");
   if (!colorTableFile.isEmpty())
   {
      std::shared_ptr<common::ColorTable> colorTable =
         common::ColorTable::Load(colorTableFile.toUtf8().constData());
      radarView->LoadColorTable(colorTable);
   }

   // QMapboxGL::addCustomLayer will take ownership of the QScopedPointer
   QScopedPointer<QMapbox::CustomLayerHostInterface> pHost(
      new RadarLayer(radarView, p->gl_));

   QString before = "ferry";

   for (const QString& layer : p->map_->layerIds())
   {
      // Draw below tunnels, ferries and roads
      if (layer.startsWith("tunnel") || layer.startsWith("ferry") ||
          layer.startsWith("road"))
      {
         before = layer;
         break;
      }
   }

   p->map_->addCustomLayer("radar", pHost, before);
   RadarRangeLayer::Add(p->map_, before);
}

void MapWidget::keyPressEvent(QKeyEvent* ev)
{
   switch (ev->key())
   {
   case Qt::Key_S: changeStyle(); break;
   case Qt::Key_L:
   {
      for (const QString& layer : p->map_->layerIds())
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
   p->lastPos_ = ev->position();

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
         p->map_->scaleBy(2.0, p->lastPos_);
      }
      else if (ev->buttons() == Qt::RightButton)
      {
         p->map_->scaleBy(0.5, p->lastPos_);
      }
   }

   ev->accept();
}

void MapWidget::mouseMoveEvent(QMouseEvent* ev)
{
   QPointF delta = ev->position() - p->lastPos_;

   if (!delta.isNull())
   {
      if (ev->buttons() == Qt::LeftButton &&
          ev->modifiers() & Qt::ShiftModifier)
      {
         p->map_->pitchBy(delta.y());
      }
      else if (ev->buttons() == Qt::LeftButton)
      {
         p->map_->moveBy(delta);
      }
      else if (ev->buttons() == Qt::RightButton)
      {
         p->map_->rotateBy(p->lastPos_, ev->position());
      }
   }

   p->lastPos_ = ev->position();
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

   p->map_->scaleBy(1 + factor, ev->position());

   ev->accept();
}

void MapWidget::initializeGL()
{
   makeCurrent();
   p->gl_.initializeOpenGLFunctions();

   p->map_.reset(new QMapboxGL(nullptr, p->settings_, size(), pixelRatio()));
   connect(p->map_.get(), SIGNAL(needsRendering()), this, SLOT(update()));

   // Set default location to KLSX.
   p->map_->setCoordinateZoom(QMapbox::Coordinate(38.6986, -90.6828), 11);

   QString styleUrl = qgetenv("MAPBOX_STYLE_URL");
   if (styleUrl.isEmpty())
   {
      changeStyle();
   }
   else
   {
      p->map_->setStyleUrl(styleUrl);
      setWindowTitle(QString("Mapbox GL: ") + styleUrl);
   }

   connect(p->map_.get(), &QMapboxGL::mapChanged, this, &MapWidget::mapChanged);
}

void MapWidget::paintGL()
{
   p->frameDraws_++;
   p->map_->resize(size());
   p->map_->setFramebufferObject(defaultFramebufferObject(),
                                 size() * pixelRatio());
   p->map_->render();
}

void MapWidget::mapChanged(QMapboxGL::MapChange mapChange)
{
   switch (mapChange)
   {
   case QMapboxGL::MapChangeDidFinishLoadingStyle: AddLayers(); break;
   }
}

} // namespace qt
} // namespace scwx
