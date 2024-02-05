#include <scwx/qt/map/overlay_product_layer.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ = "scwx::qt::map::overlay_product_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class OverlayProductLayer::Impl
{
public:
   explicit Impl(std::shared_ptr<MapContext> context) {}
   ~Impl() = default;
};

OverlayProductLayer::OverlayProductLayer(std::shared_ptr<MapContext> context) :
    DrawLayer(context), p(std::make_unique<Impl>(context))
{
}

OverlayProductLayer::~OverlayProductLayer() = default;

void OverlayProductLayer::Initialize()
{
   logger_->debug("Initialize()");

   DrawLayer::Initialize();
}

void OverlayProductLayer::Render(
   const QMapLibreGL::CustomLayerRenderParameters& params)
{
   gl::OpenGLFunctions& gl = context()->gl();

   // Set OpenGL blend mode for transparency
   gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   DrawLayer::Render(params);

   SCWX_GL_CHECK_ERROR();
}

void OverlayProductLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");

   DrawLayer::Deinitialize();
}

bool OverlayProductLayer::RunMousePicking(
   const QMapLibreGL::CustomLayerRenderParameters& params,
   const QPointF&                                  mouseLocalPos,
   const QPointF&                                  mouseGlobalPos,
   const glm::vec2&                                mouseCoords,
   const common::Coordinate&                       mouseGeoCoords,
   std::shared_ptr<types::EventHandler>&           eventHandler)
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
