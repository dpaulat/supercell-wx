#include <scwx/qt/map/poi_layer.hpp>
#include <scwx/qt/manager/poi_manager.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/qt/types/poi_types.hpp>
#include <scwx/qt/types/texture_types.hpp>
#include <scwx/qt/gl/draw/geo_icons.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ = "scwx::qt::map::poi_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);


class POILayer::Impl
{
public:
   explicit Impl(std::shared_ptr<MapContext> context) :
       geoIcons_ {std::make_shared<gl::draw::GeoIcons>(context)}
   {
   }
   ~Impl() {}

   void ReloadPOIs();

   const std::string& poiIconName_ {
      types::GetTextureName(types::ImageTexture::Cursor17)};

   std::shared_ptr<gl::draw::GeoIcons>  geoIcons_;
};

void POILayer::Impl::ReloadPOIs()
{
   auto poiManager = manager::POIManager::Instance();

   geoIcons_->StartIcons();

   for (size_t i = 0; i < poiManager->poi_count(); i++)
   {
      types::PointOfInterest poi = poiManager->get_poi(i);
      std::shared_ptr<gl::draw::GeoIconDrawItem> icon = geoIcons_->AddIcon();
      geoIcons_->SetIconTexture(icon, poiIconName_, 0);
      geoIcons_->SetIconLocation(icon, poi.latitude_, poi.longitude_);
   }

   geoIcons_->FinishIcons();
}

POILayer::POILayer(const std::shared_ptr<MapContext>& context) :
   DrawLayer(context),
   p(std::make_unique<POILayer::Impl>(context))
{
   AddDrawItem(p->geoIcons_);
}

POILayer::~POILayer() = default;

void POILayer::Initialize()
{
   logger_->debug("Initialize()");
   DrawLayer::Initialize();

   p->geoIcons_->StartIconSheets();
   p->geoIcons_->AddIconSheet(p->poiIconName_);
   p->geoIcons_->FinishIconSheets();
}

void POILayer::Render(
      const QMapLibre::CustomLayerRenderParameters& params)
{
   //auto poiManager = manager::POIManager::Instance();
   gl::OpenGLFunctions& gl = context()->gl();

   // TODO. do not redo this every time
   p->ReloadPOIs();

   DrawLayer::Render(params);

   SCWX_GL_CHECK_ERROR();
}

void POILayer::Deinitialize()
{
   logger_->debug("Deinitialize()");

   DrawLayer::Deinitialize();
}

} // namespace map
} // namespace qt
} // namespace scwx

