#include <scwx/qt/map/marker_layer.hpp>
#include <scwx/qt/manager/marker_manager.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/qt/types/marker_types.hpp>
#include <scwx/qt/types/texture_types.hpp>
#include <scwx/qt/gl/draw/geo_icons.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ = "scwx::qt::map::marker_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class MarkerLayer::Impl
{
public:
   explicit Impl(MarkerLayer* self, std::shared_ptr<MapContext> context) :
       self_ {self}, geoIcons_ {std::make_shared<gl::draw::GeoIcons>(context)}
   {
      ConnectSignals();
   }
   ~Impl() {}

   void ReloadMarkers();
   void ConnectSignals();

   MarkerLayer* self_;
   const std::string& markerIconName_ {
      types::GetTextureName(types::ImageTexture::Cursor17)};

   std::shared_ptr<gl::draw::GeoIcons> geoIcons_;
};

void MarkerLayer::Impl::ConnectSignals()
{
   auto markerManager = manager::MarkerManager::Instance();

   QObject::connect(markerManager.get(),
         &manager::MarkerManager::MarkersUpdated,
         self_,
         [this]()
         {
            this->ReloadMarkers();
         });
}

void MarkerLayer::Impl::ReloadMarkers()
{
   logger_->debug("ReloadMarkers()");
   auto markerManager = manager::MarkerManager::Instance();

   geoIcons_->StartIcons();

   for (size_t i = 0; i < markerManager->marker_count(); i++)
   {
      std::optional<types::MarkerInfo> marker = markerManager->get_marker(i);
      if (!marker)
      {
         break;
      }
      std::shared_ptr<gl::draw::GeoIconDrawItem> icon = geoIcons_->AddIcon();
      geoIcons_->SetIconTexture(icon, markerIconName_, 0);
      geoIcons_->SetIconLocation(icon, marker->latitude, marker->longitude);
   }

   geoIcons_->FinishIcons();
}

MarkerLayer::MarkerLayer(const std::shared_ptr<MapContext>& context) :
    DrawLayer(context), p(std::make_unique<MarkerLayer::Impl>(this, context))
{
   AddDrawItem(p->geoIcons_);
}

MarkerLayer::~MarkerLayer() = default;

void MarkerLayer::Initialize()
{
   logger_->debug("Initialize()");
   DrawLayer::Initialize();

   p->geoIcons_->StartIconSheets();
   p->geoIcons_->AddIconSheet(p->markerIconName_);
   p->geoIcons_->FinishIconSheets();

   p->ReloadMarkers();
}

void MarkerLayer::Render(const QMapLibre::CustomLayerRenderParameters& params)
{
   // auto markerManager = manager::MarkerManager::Instance();
   gl::OpenGLFunctions& gl = context()->gl();

   DrawLayer::Render(params);

   SCWX_GL_CHECK_ERROR();
}

void MarkerLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");

   DrawLayer::Deinitialize();
}

} // namespace map
} // namespace qt
} // namespace scwx
