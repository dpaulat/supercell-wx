#include <scwx/qt/map/overlay_layer.hpp>
#include <scwx/qt/gl/draw/rectangle.hpp>
#include <scwx/qt/gl/shader_program.hpp>
#include <scwx/qt/gl/text_shader.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <chrono>
#include <execution>

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#include <boost/date_time.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/timer/timer.hpp>
#include <GeographicLib/Geodesic.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <mbgl/util/constants.hpp>

#if !defined(_MSC_VER)
#   include <date/date.h>
#endif

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ = "scwx::qt::map::overlay_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class OverlayLayerImpl
{
public:
   explicit OverlayLayerImpl(std::shared_ptr<MapContext> context) :
       activeBoxOuter_ {std::make_shared<gl::draw::Rectangle>(context)},
       activeBoxInner_ {std::make_shared<gl::draw::Rectangle>(context)},
       sweepTimeString_ {},
       sweepTimeNeedsUpdate_ {true}
   {
   }
   ~OverlayLayerImpl() = default;

   std::shared_ptr<gl::draw::Rectangle> activeBoxOuter_;
   std::shared_ptr<gl::draw::Rectangle> activeBoxInner_;

   std::string sweepTimeString_;
   bool        sweepTimeNeedsUpdate_;
};

OverlayLayer::OverlayLayer(std::shared_ptr<MapContext> context) :
    DrawLayer(context), p(std::make_unique<OverlayLayerImpl>(context))
{
   AddDrawItem(p->activeBoxOuter_);
   AddDrawItem(p->activeBoxInner_);

   p->activeBoxOuter_->SetPosition(0.0f, 0.0f);
}

OverlayLayer::~OverlayLayer() = default;

void OverlayLayer::Initialize()
{
   logger_->debug("Initialize()");

   DrawLayer::Initialize();

   auto radarProductView = context()->radar_product_view();

   if (radarProductView != nullptr)
   {
      connect(radarProductView.get(),
              &view::RadarProductView::SweepComputed,
              this,
              &OverlayLayer::UpdateSweepTimeNextFrame);
   }
}

void OverlayLayer::Render(
   const QMapLibreGL::CustomLayerRenderParameters& params)
{
   gl::OpenGLFunctions& gl               = context()->gl();
   auto                 radarProductView = context()->radar_product_view();
   auto&                settings         = context()->settings();
   const float          pixelRatio       = context()->pixel_ratio();

   context()->set_render_parameters(params);

   if (p->sweepTimeNeedsUpdate_ && radarProductView != nullptr)
   {
      const scwx::util::time_zone* currentZone;

#if defined(_MSC_VER)
      currentZone = std::chrono::current_zone();
#else
      currentZone = date::current_zone();
#endif

      p->sweepTimeString_ = scwx::util::TimeString(
         radarProductView->sweep_time(), currentZone, false);
      p->sweepTimeNeedsUpdate_ = false;
   }

   // Active Box
   p->activeBoxOuter_->SetVisible(settings.isActive_);
   p->activeBoxInner_->SetVisible(settings.isActive_);
   if (settings.isActive_)
   {
      p->activeBoxOuter_->SetSize(params.width, params.height);
      p->activeBoxInner_->SetSize(params.width - (2.0f * pixelRatio),
                                  params.height - (2.0f * pixelRatio));

      p->activeBoxInner_->SetPosition(1.0f * pixelRatio, 1.0f * pixelRatio);

      p->activeBoxOuter_->SetBorder(1.0f * pixelRatio, {0, 0, 0, 255});
      p->activeBoxInner_->SetBorder(1.0f * pixelRatio, {255, 255, 255, 255});
   }

   DrawLayer::Render(params);

   if (radarProductView != nullptr)
   {
      // Render product name
      std::string productName = radarProductView->GetRadarProductName();
      if (productName.length() > 0 && !productName.starts_with('?'))
      {
         ImGui::SetNextWindowPos(ImVec2 {0.0f, 0.0f});
         ImGui::Begin("Product Name",
                      nullptr,
                      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_AlwaysAutoResize);
         ImGui::TextUnformatted(productName.c_str());
         ImGui::End();
      }
   }

   if (p->sweepTimeString_.length() > 0)
   {
      // Render time
      ImGui::SetNextWindowPos(ImVec2 {static_cast<float>(params.width), 0.0f},
                              ImGuiCond_Always,
                              ImVec2 {1.0f, 0.0f});
      ImGui::Begin("Sweep Time",
                   nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                      ImGuiWindowFlags_AlwaysAutoResize);
      ImGui::TextUnformatted(p->sweepTimeString_.c_str());
      ImGui::End();
   }

   SCWX_GL_CHECK_ERROR();
}

void OverlayLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");

   DrawLayer::Deinitialize();

   auto radarProductView = context()->radar_product_view();

   if (radarProductView != nullptr)
   {
      disconnect(radarProductView.get(),
                 &view::RadarProductView::SweepComputed,
                 this,
                 &OverlayLayer::UpdateSweepTimeNextFrame);
   }
}

void OverlayLayer::UpdateSweepTimeNextFrame()
{
   p->sweepTimeNeedsUpdate_ = true;
}

} // namespace map
} // namespace qt
} // namespace scwx
