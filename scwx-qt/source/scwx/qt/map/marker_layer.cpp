#include <scwx/qt/map/marker_layer.hpp>
#include <scwx/qt/manager/marker_manager.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/qt/gl/draw/geo_icons.hpp>
#include <scwx/qt/types/marker_types.hpp>
#include <scwx/qt/ui/edit_marker_dialog.hpp>

#include <string>

#include <QGeoPositionInfo>
#include <QMouseEvent>

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::marker_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class MarkerLayer::Impl
{
public:
   explicit Impl(MarkerLayer* self,
                 const std::shared_ptr<render::RenderContext>& renderContext) :
       self_ {self},
       geoIcons_ {std::make_shared<gl::draw::GeoIcons>(renderContext)},
       editMarkerDialog_ {std::make_shared<ui::EditMarkerDialog>()}
   {
      ConnectSignals();
   }
   ~Impl() = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void ReloadMarkers();
   void ConnectSignals();

   std::shared_ptr<manager::MarkerManager> markerManager_ {
      manager::MarkerManager::Instance()};

   void set_icon_sheets();

   MarkerLayer* self_;

   std::shared_ptr<gl::draw::GeoIcons>   geoIcons_;
   std::shared_ptr<ui::EditMarkerDialog> editMarkerDialog_;
};

void MarkerLayer::Impl::ConnectSignals()
{
   QObject::connect(markerManager_.get(),
                    &manager::MarkerManager::MarkersUpdated,
                    self_,
                    [this]() { ReloadMarkers(); });
   QObject::connect(markerManager_.get(),
                    &manager::MarkerManager::IconsReady,
                    self_,
                    [this]() { set_icon_sheets(); });
   QObject::connect(markerManager_.get(),
                    &manager::MarkerManager::IconAdded,
                    self_,
                    [this]() { set_icon_sheets(); });
}

void MarkerLayer::Impl::ReloadMarkers()
{
   logger_->debug("ReloadMarkers()");

   geoIcons_->StartIcons();
   markerManager_->for_each(
      [this](const types::MarkerInfo& marker)
      {
         // must use local ID, instead of reference to marker in event handler
         // callback.
         const types::MarkerId id = marker.id;

         const std::shared_ptr<gl::draw::GeoIconDrawItem> icon =
            geoIcons_->AddIcon();

         const std::string latitudeString =
            common::GetLatitudeString(marker.latitude);
         const std::string longitudeString =
            common::GetLongitudeString(marker.longitude);

         const std::string hoverText =
            marker.name != "" ?
               fmt::format(
                  "{}\n{}, {}", marker.name, latitudeString, longitudeString) :
               fmt::format("{}, {}", latitudeString, longitudeString);

         auto iconInfo = markerManager_->get_icon(marker.iconName);
         if (iconInfo)
         {
            geoIcons_->SetIconTexture(icon, iconInfo->name, 0);
         }
         else
         {
            geoIcons_->SetIconTexture(icon, marker.iconName, 0);
         }

         geoIcons_->SetIconLocation(icon, marker.latitude, marker.longitude);
         geoIcons_->SetIconHoverText(icon, hoverText);
         geoIcons_->SetIconModulate(icon, marker.iconColor);
         geoIcons_->RegisterEventHandler(
            icon,
            [this, id](QEvent* ev)
            {
               switch (ev->type())
               {
               case QEvent::Type::MouseButtonPress:
               {
                  auto* mouseEvent = reinterpret_cast<QMouseEvent*>(ev);
                  if (mouseEvent->buttons() == Qt::MouseButton::RightButton)
                  {
                     editMarkerDialog_->setup(id);
                     editMarkerDialog_->show();
                  }
               }
               break;

               default:
                  break;
               }
            });
      });

   geoIcons_->FinishIcons();
   Q_EMIT self_->NeedsRendering();
}

MarkerLayer::MarkerLayer(
   const std::shared_ptr<render::RenderContext>& renderContext) :
    DrawLayer(renderContext, "MarkerLayer"),
    p(std::make_unique<MarkerLayer::Impl>(this, renderContext))
{
   AddDrawItem(p->geoIcons_);
}

MarkerLayer::~MarkerLayer() = default;

void MarkerLayer::Initialize(const std::shared_ptr<MapContext>& mapContext)
{
   logger_->debug("Initialize()");
   DrawLayer::Initialize(mapContext);

   p->set_icon_sheets();
   p->ReloadMarkers();
}

void MarkerLayer::Impl::set_icon_sheets()
{
   geoIcons_->StartIconSheets();
   for (auto& markerIcon : markerManager_->get_icons())
   {
      geoIcons_->AddIconSheet(markerIcon.second.name,
                              0,
                              0,
                              markerIcon.second.hotX,
                              markerIcon.second.hotY);
   }
   geoIcons_->FinishIconSheets();
}

void MarkerLayer::Render(const std::shared_ptr<MapContext>& mapContext,
                         const QMapLibre::CustomLayerRenderParameters& params)
{
   DrawLayer::Render(mapContext, params);

}

void MarkerLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");

   DrawLayer::Deinitialize();
}

} // namespace scwx::qt::map
