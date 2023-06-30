#include <scwx/qt/map/map_widget.hpp>
#include <scwx/qt/gl/gl.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/map/alert_layer.hpp>
#include <scwx/qt/map/color_table_layer.hpp>
#include <scwx/qt/map/layer_wrapper.hpp>
#include <scwx/qt/map/map_provider.hpp>
#include <scwx/qt/map/overlay_layer.hpp>
#include <scwx/qt/map/radar_product_layer.hpp>
#include <scwx/qt/map/radar_range_layer.hpp>
#include <scwx/qt/model/imgui_context_model.hpp>
#include <scwx/qt/util/file.hpp>
#include <scwx/qt/view/radar_product_view_factory.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <regex>

#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_qt.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/uuid/random_generator.hpp>
#include <fmt/format.h>
#include <imgui.h>
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
namespace map
{

static const std::string logPrefix_ = "scwx::qt::map::map_widget";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class MapWidgetImpl : public QObject
{
   Q_OBJECT

public:
   explicit MapWidgetImpl(MapWidget*                   widget,
                          const QMapLibreGL::Settings& settings) :
       uuid_ {boost::uuids::random_generator()()},
       context_ {std::make_shared<MapContext>()},
       widget_ {widget},
       settings_(settings),
       map_(),
       layerList_ {},
       imGuiRendererInitialized_ {false},
       radarProductManager_ {nullptr},
       radarProductLayer_ {nullptr},
       alertLayer_ {std::make_shared<AlertLayer>(context_)},
       overlayLayer_ {nullptr},
       colorTableLayer_ {nullptr},
       autoRefreshEnabled_ {true},
       autoUpdateEnabled_ {true},
       selectedLevel2Product_ {common::Level2Product::Unknown},
       lastPos_(),
       currentStyleIndex_ {0},
       currentStyle_ {nullptr},
       frameDraws_(0),
       prevLatitude_ {0.0},
       prevLongitude_ {0.0},
       prevZoom_ {0.0},
       prevBearing_ {0.0},
       prevPitch_ {0.0}
   {
      auto& generalSettings =
         scwx::qt::manager::SettingsManager::general_settings();

      SetRadarSite(generalSettings.default_radar_site().GetValue());

      // Create ImGui Context
      static size_t currentMapId_ {0u};
      imGuiContextName_ = fmt::format("Map {}", ++currentMapId_);
      imGuiContext_ =
         model::ImGuiContextModel::Instance().CreateContext(imGuiContextName_);

      // Initialize ImGui Qt backend
      ImGui_ImplQt_Init();
      ImGui_ImplQt_RegisterWidget(widget_);

      // Set Map Provider Details
      mapProvider_ = GetMapProvider(generalSettings.map_provider().GetValue());
   }

   ~MapWidgetImpl()
   {
      // Set ImGui Context
      ImGui::SetCurrentContext(imGuiContext_);

      // Shutdown ImGui Context
      if (imGuiRendererInitialized_)
      {
         ImGui_ImplOpenGL3_Shutdown();
      }
      ImGui_ImplQt_Shutdown();

      // Destroy ImGui Context
      model::ImGuiContextModel::Instance().DestroyContext(imGuiContextName_);
   }

   void AddLayer(const std::string&            id,
                 std::shared_ptr<GenericLayer> layer,
                 const std::string&            before = {});
   void InitializeNewRadarProductView(const std::string& colorPalette);
   void RadarProductManagerConnect();
   void RadarProductManagerDisconnect();
   void RadarProductViewConnect();
   void RadarProductViewDisconnect();
   void SetRadarSite(const std::string& radarSite);
   bool UpdateStoredMapParameters();

   common::Level2Product
   GetLevel2ProductOrDefault(const std::string& productName) const;

   boost::asio::thread_pool threadPool_ {1u};

   boost::uuids::uuid uuid_;

   std::shared_ptr<MapContext> context_;

   MapWidget*                        widget_;
   MapProvider                       mapProvider_;
   QMapLibreGL::Settings             settings_;
   std::shared_ptr<QMapLibreGL::Map> map_;
   std::list<std::string>            layerList_;

   ImGuiContext* imGuiContext_;
   std::string   imGuiContextName_;
   bool          imGuiRendererInitialized_;

   std::shared_ptr<manager::RadarProductManager> radarProductManager_;

   std::shared_ptr<common::ColorTable> colorTable_;

   std::shared_ptr<RadarProductLayer> radarProductLayer_;
   std::shared_ptr<AlertLayer>        alertLayer_;
   std::shared_ptr<OverlayLayer>      overlayLayer_;
   std::shared_ptr<ColorTableLayer>   colorTableLayer_;

   bool autoRefreshEnabled_;
   bool autoUpdateEnabled_;

   common::Level2Product selectedLevel2Product_;

   QPointF         lastPos_;
   std::size_t     currentStyleIndex_;
   const MapStyle* currentStyle_;
   std::string     initialStyleName_ {};

   uint64_t frameDraws_;

   double prevLatitude_;
   double prevLongitude_;
   double prevZoom_;
   double prevBearing_;
   double prevPitch_;

public slots:
   void Update();
};

MapWidget::MapWidget(const QMapLibreGL::Settings& settings) :
    p(std::make_unique<MapWidgetImpl>(this, settings))
{
   setFocusPolicy(Qt::StrongFocus);

   ImGui_ImplQt_RegisterWidget(this);
}

MapWidget::~MapWidget()
{
   // Make sure we have a valid context so we can delete the QMapLibreGL.
   makeCurrent();
}

common::Level3ProductCategoryMap MapWidget::GetAvailableLevel3Categories()
{
   if (p->radarProductManager_ != nullptr)
   {
      return p->radarProductManager_->GetAvailableLevel3Categories();
   }
   else
   {
      return {};
   }
}

float MapWidget::GetElevation() const
{
   auto radarProductView = p->context_->radar_product_view();

   if (radarProductView != nullptr)
   {
      return radarProductView->elevation();
   }
   else
   {
      return 0.0f;
   }
}

std::vector<float> MapWidget::GetElevationCuts() const
{
   auto radarProductView = p->context_->radar_product_view();

   if (radarProductView != nullptr)
   {
      return radarProductView->GetElevationCuts();
   }
   else
   {
      return {};
   }
}

common::Level2Product
MapWidgetImpl::GetLevel2ProductOrDefault(const std::string& productName) const
{
   common::Level2Product level2Product = common::GetLevel2Product(productName);

   if (level2Product == common::Level2Product::Unknown)
   {
      auto radarProductView = context_->radar_product_view();

      if (radarProductView != nullptr)
      {
         level2Product =
            common::GetLevel2Product(radarProductView->GetRadarProductName());
      }
   }

   if (level2Product == common::Level2Product::Unknown)
   {
      if (selectedLevel2Product_ != common::Level2Product::Unknown)
      {
         level2Product = selectedLevel2Product_;
      }
      else
      {
         level2Product = common::Level2Product::Reflectivity;
      }
   }

   return level2Product;
}

std::vector<std::string> MapWidget::GetLevel3Products()
{
   if (p->radarProductManager_ != nullptr)
   {
      return p->radarProductManager_->GetLevel3Products();
   }
   else
   {
      return {};
   }
}

std::string MapWidget::GetMapStyle() const
{
   if (p->currentStyle_ != nullptr)
   {
      return p->currentStyle_->name_;
   }
   else
   {
      return "?";
   }
}

common::RadarProductGroup MapWidget::GetRadarProductGroup() const
{
   auto radarProductView = p->context_->radar_product_view();

   if (radarProductView != nullptr)
   {
      return radarProductView->GetRadarProductGroup();
   }
   else
   {
      return common::RadarProductGroup::Unknown;
   }
}

std::string MapWidget::GetRadarProductName() const
{
   auto radarProductView = p->context_->radar_product_view();

   if (radarProductView != nullptr)
   {
      return radarProductView->GetRadarProductName();
   }
   else
   {
      return "?";
   }
}

std::shared_ptr<config::RadarSite> MapWidget::GetRadarSite() const
{
   std::shared_ptr<config::RadarSite> radarSite = nullptr;

   if (p->radarProductManager_ != nullptr)
   {
      radarSite = p->radarProductManager_->radar_site();
   }

   return radarSite;
}

std::chrono::system_clock::time_point MapWidget::GetSelectedTime() const
{
   auto radarProductView = p->context_->radar_product_view();
   std::chrono::system_clock::time_point time;

   // If there is an active radar product view
   if (radarProductView != nullptr)
   {
      // Select the time associated with the active radar product
      time = radarProductView->GetSelectedTime();
   }

   return time;
}

std::uint16_t MapWidget::GetVcp() const
{
   auto radarProductView = p->context_->radar_product_view();

   if (radarProductView != nullptr)
   {
      return radarProductView->vcp();
   }
   else
   {
      return 0u;
   }
}

void MapWidget::SelectElevation(float elevation)
{
   auto radarProductView = p->context_->radar_product_view();

   if (radarProductView != nullptr)
   {
      radarProductView->SelectElevation(elevation);
      radarProductView->Update();
   }
}

void MapWidget::SelectRadarProduct(common::RadarProductGroup group,
                                   const std::string&        product,
                                   std::int16_t              productCode,
                                   std::chrono::system_clock::time_point time,
                                   bool                                  update)
{
   bool radarProductViewCreated = false;

   auto radarProductView = p->context_->radar_product_view();

   std::string productName {product};

   // Validate level 2 product, set to default if invalid
   if (group == common::RadarProductGroup::Level2)
   {
      common::Level2Product level2Product =
         p->GetLevel2ProductOrDefault(productName);
      productName               = common::GetLevel2Name(level2Product);
      p->selectedLevel2Product_ = level2Product;
   }

   if (group == common::RadarProductGroup::Level3 && productCode == 0)
   {
      productCode = common::GetLevel3ProductCodeByAwipsId(productName);
   }

   if (radarProductView == nullptr ||
       radarProductView->GetRadarProductGroup() != group ||
       (radarProductView->GetRadarProductGroup() ==
           common::RadarProductGroup::Level2 &&
        radarProductView->GetRadarProductName() != productName) ||
       p->context_->radar_product_code() != productCode)
   {
      p->RadarProductViewDisconnect();

      radarProductView = view::RadarProductViewFactory::Create(
         group, productName, productCode, p->radarProductManager_);
      p->context_->set_radar_product_view(radarProductView);

      p->RadarProductViewConnect();

      radarProductViewCreated = true;
   }
   else
   {
      radarProductView->SelectProduct(productName);
   }

   p->context_->set_radar_product_group(group);
   p->context_->set_radar_product(productName);
   p->context_->set_radar_product_code(productCode);

   if (radarProductView != nullptr)
   {
      // Select the time associated with the request
      radarProductView->SelectTime(time);

      if (radarProductViewCreated)
      {
         const std::string palette =
            (group == common::RadarProductGroup::Level2) ?
               common::GetLevel2Palette(common::GetLevel2Product(productName)) :
               common::GetLevel3Palette(productCode);
         p->InitializeNewRadarProductView(palette);
      }
      else if (update)
      {
         radarProductView->Update();
      }
   }

   if (p->autoRefreshEnabled_)
   {
      p->radarProductManager_->EnableRefresh(
         group, productName, true, p->uuid_);
   }
}

void MapWidget::SelectRadarProduct(
   std::shared_ptr<types::RadarProductRecord> record)
{
   const std::string                     radarId = record->radar_id();
   common::RadarProductGroup             group = record->radar_product_group();
   const std::string                     product     = record->radar_product();
   std::chrono::system_clock::time_point time        = record->time();
   int16_t                               productCode = record->product_code();

   logger_->debug("SelectRadarProduct: {}, {}, {}, {}",
                  radarId,
                  common::GetRadarProductGroupName(group),
                  product,
                  scwx::util::TimeString(time));

   p->SetRadarSite(radarId);

   SelectRadarProduct(group, product, productCode, time);
}

void MapWidget::SelectRadarSite(const std::string& id, bool updateCoordinates)
{
   logger_->debug("Selecting radar site: {}", id);

   std::shared_ptr<config::RadarSite> radarSite = config::RadarSite::Get(id);

   SelectRadarSite(radarSite, updateCoordinates);
}

void MapWidget::SelectRadarSite(std::shared_ptr<config::RadarSite> radarSite,
                                bool updateCoordinates)
{
   // Verify radar site is valid and has changed
   if (radarSite != nullptr &&
       (p->radarProductManager_ == nullptr ||
        radarSite->id() != p->radarProductManager_->radar_site()->id()))
   {
      auto radarProductView = p->context_->radar_product_view();

      if (updateCoordinates)
      {
         p->map_->setCoordinate(
            {radarSite->latitude(), radarSite->longitude()});
      }
      p->SetRadarSite(radarSite->id());
      p->Update();

      // Select products from new site
      if (radarProductView != nullptr)
      {
         radarProductView->set_radar_product_manager(p->radarProductManager_);
         SelectRadarProduct(radarProductView->GetRadarProductGroup(),
                            radarProductView->GetRadarProductName(),
                            0,
                            radarProductView->selected_time(),
                            false);
      }

      AddLayers();

      // TODO: Disable refresh from old site

      Q_EMIT RadarSiteUpdated(radarSite);
   }
}

void MapWidget::SelectTime(std::chrono::system_clock::time_point time)
{
   auto radarProductView = p->context_->radar_product_view();

   // If there is an active radar product view
   if (radarProductView != nullptr)
   {
      // Select the time associated with the active radar product
      radarProductView->SelectTime(time);

      // Trigger an update of the radar product view
      radarProductView->Update();
   }
}

void MapWidget::SetActive(bool isActive)
{
   p->context_->settings().isActive_ = isActive;
   update();
}

void MapWidget::SetAutoRefresh(bool enabled)
{
   if (p->autoRefreshEnabled_ != enabled)
   {
      p->autoRefreshEnabled_ = enabled;

      auto radarProductView = p->context_->radar_product_view();

      if (p->autoRefreshEnabled_ && radarProductView != nullptr)
      {
         p->radarProductManager_->EnableRefresh(
            radarProductView->GetRadarProductGroup(),
            radarProductView->GetRadarProductName(),
            true,
            p->uuid_);
      }
   }
}

void MapWidget::SetAutoUpdate(bool enabled)
{
   p->autoUpdateEnabled_ = enabled;
}

void MapWidget::SetMapLocation(double latitude,
                               double longitude,
                               bool   updateRadarSite)
{
   if (p->map_ != nullptr &&
       (p->prevLatitude_ != latitude || p->prevLongitude_ != longitude))
   {
      // Update the map location
      p->map_->setCoordinate({latitude, longitude});

      // If the radar site should be updated based on the new location
      if (updateRadarSite)
      {
         // Find the nearest WSR-88D radar
         std::shared_ptr<config::RadarSite> nearestRadarSite =
            config::RadarSite::FindNearest(latitude, longitude, "wsr88d");

         // If found, select it
         if (nearestRadarSite != nullptr)
         {
            SelectRadarSite(nearestRadarSite->id(), false);
         }
      }
   }
}

void MapWidget::SetMapParameters(
   double latitude, double longitude, double zoom, double bearing, double pitch)
{
   if (p->map_ != nullptr &&
       (p->prevLatitude_ != latitude || p->prevLongitude_ != longitude ||
        p->prevZoom_ != zoom || p->prevBearing_ != bearing ||
        p->prevPitch_ != pitch))
   {
      p->map_->setCoordinateZoom({latitude, longitude}, zoom);
      p->map_->setBearing(bearing);
      p->map_->setPitch(pitch);
   }
}

void MapWidget::SetInitialMapStyle(const std::string& styleName)
{
   p->initialStyleName_ = styleName;
}

void MapWidget::SetMapStyle(const std::string& styleName)
{
   const auto& mapProviderInfo = GetMapProviderInfo(p->mapProvider_);
   auto&       styles          = mapProviderInfo.mapStyles_;

   for (size_t i = 0u; i < styles.size(); ++i)
   {
      if (styles[i].name_ == styleName)
      {
         p->currentStyleIndex_ = i;
         p->currentStyle_      = &styles[i];

         logger_->debug("Updating style: {}", styles[i].name_);

         p->map_->setStyleUrl(styles[i].url_.c_str());

         if (++p->currentStyleIndex_ == styles.size())
         {
            p->currentStyleIndex_ = 0;
         }

         break;
      }
   }
}

qreal MapWidget::pixelRatio()
{
   return devicePixelRatioF();
}

void MapWidget::changeStyle()
{
   const auto& mapProviderInfo = GetMapProviderInfo(p->mapProvider_);
   auto&       styles          = mapProviderInfo.mapStyles_;

   p->currentStyle_ = &styles[p->currentStyleIndex_];

   logger_->debug("Updating style: {}", styles[p->currentStyleIndex_].name_);

   p->map_->setStyleUrl(styles[p->currentStyleIndex_].url_.c_str());

   if (++p->currentStyleIndex_ == styles.size())
   {
      p->currentStyleIndex_ = 0;
   }

   Q_EMIT MapStyleChanged(p->currentStyle_->name_);
}

void MapWidget::AddLayers()
{
   logger_->debug("AddLayers()");

   // Clear custom layers
   for (const std::string& id : p->layerList_)
   {
      p->map_->removeLayer(id.c_str());
   }
   p->layerList_.clear();

   auto radarProductView = p->context_->radar_product_view();

   if (radarProductView != nullptr)
   {
      p->radarProductLayer_ = std::make_shared<RadarProductLayer>(p->context_);
      p->colorTableLayer_   = std::make_shared<ColorTableLayer>(p->context_);

      std::shared_ptr<config::RadarSite> radarSite =
         p->radarProductManager_->radar_site();

      const auto& mapStyle = *p->currentStyle_;

      std::string before = "ferry";

      for (const QString& qlayer : p->map_->layerIds())
      {
         const std::string layer = qlayer.toStdString();

         // Draw below layers defined in map style
         auto it = std::find_if(mapStyle.drawBelow_.cbegin(),
                                mapStyle.drawBelow_.cend(),
                                [&layer](const std::string& styleLayer) -> bool
                                {
                                   std::regex re {styleLayer};
                                   return std::regex_match(layer, re);
                                });

         if (it != mapStyle.drawBelow_.cend())
         {
            before = layer;
            break;
         }
      }

      p->AddLayer("radar", p->radarProductLayer_, before);
      RadarRangeLayer::Add(p->map_,
                           radarProductView->range(),
                           {radarSite->latitude(), radarSite->longitude()});
      p->AddLayer("colorTable", p->colorTableLayer_);
   }

   p->alertLayer_->AddLayers("colorTable");
   p->overlayLayer_ = std::make_shared<OverlayLayer>(p->context_);
   p->AddLayer("overlay", p->overlayLayer_);
}

void MapWidgetImpl::AddLayer(const std::string&            id,
                             std::shared_ptr<GenericLayer> layer,
                             const std::string&            before)
{
   // QMapLibreGL::addCustomLayer will take ownership of the std::unique_ptr
   std::unique_ptr<QMapLibreGL::CustomLayerHostInterface> pHost =
      std::make_unique<LayerWrapper>(layer);

   map_->addCustomLayer(id.c_str(), std::move(pHost), before.c_str());

   layerList_.push_back(id);
}

void MapWidget::keyPressEvent(QKeyEvent* ev)
{
   switch (ev->key())
   {
   case Qt::Key_S:
      changeStyle();
      break;
   case Qt::Key_L:
   {
      for (const QString& layer : p->map_->layerIds())
      {
         qDebug() << "Layer: " << layer;
      }
   }
   break;
   default:
      break;
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
   logger_->debug("initializeGL()");

   makeCurrent();
   p->context_->gl().initializeOpenGLFunctions();

   // Initialize ImGui OpenGL3 backend
   ImGui::SetCurrentContext(p->imGuiContext_);
   ImGui_ImplOpenGL3_Init();
   p->imGuiRendererInitialized_ = true;

   p->map_.reset(
      new QMapLibreGL::Map(nullptr, p->settings_, size(), pixelRatio()));
   p->context_->set_map(p->map_);
   connect(p->map_.get(),
           &QMapLibreGL::Map::needsRendering,
           p.get(),
           &MapWidgetImpl::Update);

   // Set default location to radar site
   std::shared_ptr<config::RadarSite> radarSite =
      p->radarProductManager_->radar_site();
   p->map_->setCoordinateZoom({radarSite->latitude(), radarSite->longitude()},
                              7);
   p->UpdateStoredMapParameters();
   Q_EMIT MapParametersChanged(p->prevLatitude_,
                               p->prevLongitude_,
                               p->prevZoom_,
                               p->prevBearing_,
                               p->prevPitch_);

   // Update style
   if (p->initialStyleName_.empty())
   {
      changeStyle();
   }
   else
   {
      SetMapStyle(p->initialStyleName_);
   }

   connect(p->map_.get(),
           &QMapLibreGL::Map::mapChanged,
           this,
           &MapWidget::mapChanged);
}

void MapWidget::paintGL()
{
   p->frameDraws_++;

   // Setup ImGui Frame
   ImGui::SetCurrentContext(p->imGuiContext_);

   // Start ImGui Frame
   ImGui_ImplQt_NewFrame(this);
   ImGui_ImplOpenGL3_NewFrame();
   ImGui::NewFrame();

   // Update pixel ratio
   p->context_->set_pixel_ratio(pixelRatio());

   // Render QMapLibreGL Map
   p->map_->resize(size());
   p->map_->setFramebufferObject(defaultFramebufferObject(),
                                 size() * pixelRatio());
   p->map_->render();

   // Render ImGui Frame
   ImGui::Render();
   ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

   // Paint complete
   Q_EMIT WidgetPainted();
}

void MapWidget::mapChanged(QMapLibreGL::Map::MapChange mapChange)
{
   switch (mapChange)
   {
   case QMapLibreGL::Map::MapChangeDidFinishLoadingStyle:
      AddLayers();
      break;

   default:
      break;
   }
}

void MapWidgetImpl::RadarProductManagerConnect()
{
   if (radarProductManager_ != nullptr)
   {
      connect(radarProductManager_.get(),
              &manager::RadarProductManager::Level3ProductsChanged,
              this,
              [this]() { Q_EMIT widget_->Level3ProductsChanged(); });

      connect(
         radarProductManager_.get(),
         &manager::RadarProductManager::NewDataAvailable,
         this,
         [this](common::RadarProductGroup             group,
                const std::string&                    product,
                std::chrono::system_clock::time_point latestTime)
         {
            if (autoRefreshEnabled_ &&
                context_->radar_product_group() == group &&
                (group == common::RadarProductGroup::Level2 ||
                 context_->radar_product() == product))
            {
               // Create file request
               std::shared_ptr<request::NexradFileRequest> request =
                  std::make_shared<request::NexradFileRequest>();

               // File request callback
               if (autoUpdateEnabled_)
               {
                  connect(
                     request.get(),
                     &request::NexradFileRequest::RequestComplete,
                     this,
                     [=,
                      this](std::shared_ptr<request::NexradFileRequest> request)
                     {
                        // Select loaded record
                        auto record = request->radar_product_record();

                        // Validate record, and verify current map context still
                        // displays product
                        if (record != nullptr &&
                            context_->radar_product_group() == group &&
                            (group == common::RadarProductGroup::Level2 ||
                             context_->radar_product() == product))
                        {
                           widget_->SelectRadarProduct(record);
                        }
                     });
               }

               // Load file
               boost::asio::post(
                  threadPool_,
                  [=, this]()
                  {
                     if (group == common::RadarProductGroup::Level2)
                     {
                        radarProductManager_->LoadLevel2Data(latestTime,
                                                             request);
                     }
                     else
                     {
                        radarProductManager_->LoadLevel3Data(
                           product, latestTime, request);
                     }
                  });
            }
         },
         Qt::QueuedConnection);
   }
}

void MapWidgetImpl::RadarProductManagerDisconnect()
{
   if (radarProductManager_ != nullptr)
   {
      disconnect(radarProductManager_.get(),
                 &manager::RadarProductManager::NewDataAvailable,
                 this,
                 nullptr);
   }
}

void MapWidgetImpl::InitializeNewRadarProductView(
   const std::string& colorPalette)
{
   boost::asio::post(threadPool_,
                     [=, this]()
                     {
                        auto radarProductView = context_->radar_product_view();

                        std::string colorTableFile =
                           manager::SettingsManager::palette_settings()
                              .palette(colorPalette)
                              .GetValue();
                        if (!colorTableFile.empty())
                        {
                           std::unique_ptr<std::istream> colorTableStream =
                              util::OpenFile(colorTableFile);
                           std::shared_ptr<common::ColorTable> colorTable =
                              common::ColorTable::Load(*colorTableStream);
                           radarProductView->LoadColorTable(colorTable);
                        }

                        radarProductView->Initialize();
                     });

   if (map_ != nullptr)
   {
      widget_->AddLayers();
   }
}

void MapWidgetImpl::RadarProductViewConnect()
{
   auto radarProductView = context_->radar_product_view();

   if (radarProductView != nullptr)
   {
      connect(
         radarProductView.get(),
         &view::RadarProductView::ColorTableUpdated,
         this,
         [this]() { widget_->update(); },
         Qt::QueuedConnection);
      connect(
         radarProductView.get(),
         &view::RadarProductView::SweepComputed,
         this,
         [=, this]()
         {
            std::shared_ptr<config::RadarSite> radarSite =
               radarProductManager_->radar_site();

            RadarRangeLayer::Update(
               map_,
               radarProductView->range(),
               {radarSite->latitude(), radarSite->longitude()});
            widget_->update();
            Q_EMIT widget_->RadarSweepUpdated();
         },
         Qt::QueuedConnection);
      connect(radarProductView.get(),
              &view::RadarProductView::SweepNotComputed,
              widget_,
              &MapWidget::RadarSweepNotUpdated);
   }
}

void MapWidgetImpl::RadarProductViewDisconnect()
{
   auto radarProductView = context_->radar_product_view();

   if (radarProductView != nullptr)
   {
      disconnect(radarProductView.get(),
                 &view::RadarProductView::ColorTableUpdated,
                 this,
                 nullptr);
      disconnect(radarProductView.get(),
                 &view::RadarProductView::SweepComputed,
                 this,
                 nullptr);
      disconnect(radarProductView.get(),
                 &view::RadarProductView::SweepNotComputed,
                 widget_,
                 nullptr);
   }
}

void MapWidgetImpl::SetRadarSite(const std::string& radarSite)
{
   // Check if radar site has changed
   if (radarProductManager_ == nullptr ||
       radarSite != radarProductManager_->radar_site()->id())
   {
      // Disconnect signals from old RadarProductManager
      RadarProductManagerDisconnect();

      // Set new RadarProductManager
      radarProductManager_ = manager::RadarProductManager::Instance(radarSite);

      // Connect signals to new RadarProductManager
      RadarProductManagerConnect();

      radarProductManager_->UpdateAvailableProducts();
   }
}

void MapWidgetImpl::Update()
{
   widget_->update();

   if (UpdateStoredMapParameters())
   {
      Q_EMIT widget_->MapParametersChanged(
         prevLatitude_, prevLongitude_, prevZoom_, prevBearing_, prevPitch_);
   }
}

bool MapWidgetImpl::UpdateStoredMapParameters()
{
   bool changed = false;

   double newLatitude  = map_->latitude();
   double newLongitude = map_->longitude();
   double newZoom      = map_->zoom();
   double newBearing   = map_->bearing();
   double newPitch     = map_->pitch();

   if (prevLatitude_ != newLatitude ||   //
       prevLongitude_ != newLongitude || //
       prevZoom_ != newZoom ||           //
       prevBearing_ != newBearing ||     //
       prevPitch_ != newPitch)
   {
      prevLatitude_  = newLatitude;
      prevLongitude_ = newLongitude;
      prevZoom_      = newZoom;
      prevBearing_   = newBearing;
      prevPitch_     = newPitch;

      changed = true;
   }

   return changed;
}

} // namespace map
} // namespace qt
} // namespace scwx

#include "map_widget.moc"
