#include <scwx/qt/map/color_table_layer.hpp>
#include <scwx/qt/render/projection.hpp>
#include <scwx/qt/render/rhi_color_table_overlay.hpp>
#include <scwx/qt/render/rhi_vulkan_overlay.hpp>
#include <scwx/qt/view/radar_product_view.hpp>
#include <scwx/util/logger.hpp>

#include <chrono>
#include <cstring>

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::color_table_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class ColorTableLayer::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   std::vector<boost::gil::rgba8_pixel_t> colorTable_ {};

   bool colorTableNeedsUpdate_ {true};
};

ColorTableLayer::ColorTableLayer(
   std::shared_ptr<render::RenderContext> renderContext) :
    GenericLayer(std::move(renderContext)), p(std::make_unique<Impl>())
{
}
ColorTableLayer::~ColorTableLayer() = default;

void ColorTableLayer::Initialize(const std::shared_ptr<MapContext>& mapContext)
{
   logger_->debug("Initialize()");

   connect(mapContext->radar_product_view().get(),
           &view::RadarProductView::ColorTableLutUpdated,
           this,
           [this]() { p->colorTableNeedsUpdate_ = true; });
}

void ColorTableLayer::Render(
   const std::shared_ptr<MapContext>& mapContext,
   const QMapLibre::CustomLayerRenderParameters& /* params */)
{
   mapContext->set_color_table_margins(QMargins {});
}

void ColorTableLayer::RenderVulkanOverlay(
   QRhiCommandBuffer*                            commandBuffer,
   render::RhiVulkanOverlayResources&            resources,
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   auto radarProductView = mapContext->radar_product_view();

   if (radarProductView == nullptr || !radarProductView->IsInitialized())
   {
      mapContext->set_color_table_margins({});
      return;
   }

   if (p->colorTableNeedsUpdate_)
   {
      p->colorTable_            = radarProductView->color_table_lut();
      p->colorTableNeedsUpdate_ = false;
   }

   if (p->colorTable_.empty() || radarProductView->sweep_time() ==
                                    std::chrono::system_clock::time_point())
   {
      mapContext->set_color_table_margins(QMargins {});
      return;
   }

   const float vertexLX       = 0.0f;
   const float vertexRX       = static_cast<float>(params.width);
   const float vertexTY       = 10.0f;
   const float vertexBY       = 0.0f;
   const float vertices[6][2] = {{vertexLX, vertexTY},
                                 {vertexLX, vertexBY},
                                 {vertexRX, vertexTY},
                                 {vertexLX, vertexBY},
                                 {vertexRX, vertexTY},
                                 {vertexRX, vertexBY}};

   std::vector<std::uint8_t> rgbaColorTable(p->colorTable_.size() * 4);
   std::memcpy(
      rgbaColorTable.data(), p->colorTable_.data(), rgbaColorTable.size());

   resources.colorTable.Render(commandBuffer,
                               render::OrthoMapProjection(params),
                               vertices,
                               rgbaColorTable);

   static constexpr int kBottomMargin = 10;
   mapContext->set_color_table_margins(QMargins {0, 0, 0, kBottomMargin});
}

void ColorTableLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");
}

} // namespace scwx::qt::map
