#include <scwx/qt/map/alert_layer.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ = "scwx::qt::map::alert_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class AlertLayer::Impl
{
public:
   explicit Impl([[maybe_unused]] std::shared_ptr<MapContext> context,
                 awips::Phenomenon                            phenomenon) :
       phenomenon_ {phenomenon}
   {
   }
   ~Impl() {};

   awips::Phenomenon phenomenon_;
};

AlertLayer::AlertLayer(std::shared_ptr<MapContext> context,
                       awips::Phenomenon           phenomenon) :
    DrawLayer(context), p(std::make_unique<Impl>(context, phenomenon))
{
}

AlertLayer::~AlertLayer() = default;

void AlertLayer::Initialize()
{
   logger_->debug("Initialize: {}", awips::GetPhenomenonText(p->phenomenon_));

   DrawLayer::Initialize();
}

void AlertLayer::Render(const QMapLibre::CustomLayerRenderParameters& params)
{
   gl::OpenGLFunctions& gl = context()->gl();

   DrawLayer::Render(params);

   SCWX_GL_CHECK_ERROR();
}

void AlertLayer::Deinitialize()
{
   logger_->debug("Deinitialize: {}", awips::GetPhenomenonText(p->phenomenon_));

   DrawLayer::Deinitialize();
}

bool AlertLayer::RunMousePicking(
   const QMapLibre::CustomLayerRenderParameters& params,
   const QPointF&                                mouseLocalPos,
   const QPointF&                                mouseGlobalPos,
   const glm::vec2&                              mouseCoords,
   const common::Coordinate&                     mouseGeoCoords,
   std::shared_ptr<types::EventHandler>&         eventHandler)
{
   return DrawLayer::RunMousePicking(params,
                                     mouseLocalPos,
                                     mouseGlobalPos,
                                     mouseCoords,
                                     mouseGeoCoords,
                                     eventHandler);
}

} // namespace map
} // namespace qt
} // namespace scwx
