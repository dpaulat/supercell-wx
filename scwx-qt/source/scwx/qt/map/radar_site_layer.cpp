#include <scwx/qt/map/radar_site_layer.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/qt/util/tooltip.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/util/logger.hpp>

// #include <GeographicLib/Geodesic.hpp>
#include <imgui.h>
#include <mbgl/util/constants.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ = "scwx::qt::map::radar_site_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class RadarSiteLayer::Impl
{
public:
   explicit Impl(RadarSiteLayer* self) : self_ {self} {}
   ~Impl() = default;

   void RenderRadarSite(const QMapLibreGL::CustomLayerRenderParameters& params,
                        std::shared_ptr<config::RadarSite>& radarSite);

   RadarSiteLayer* self_;

   std::vector<std::shared_ptr<config::RadarSite>> radarSites_ {};

   glm::vec2 mapScreenCoordLocation_ {};
   float     mapScale_ {1.0f};
   float     mapBearingCos_ {1.0f};
   float     mapBearingSin_ {0.0f};
   float     halfWidth_ {};
   float     halfHeight_ {};

   std::string hoverText_ {};
};

RadarSiteLayer::RadarSiteLayer(std::shared_ptr<MapContext> context) :
    DrawLayer(context), p(std::make_unique<Impl>(this))
{
}

RadarSiteLayer::~RadarSiteLayer() = default;

void RadarSiteLayer::Initialize()
{
   logger_->debug("Initialize()");

   p->radarSites_ = config::RadarSite::GetAll();
}

void RadarSiteLayer::Render(
   const QMapLibreGL::CustomLayerRenderParameters& params)
{
   gl::OpenGLFunctions& gl = context()->gl();

   context()->set_render_parameters(params);

   // Update map screen coordinate and scale information
   p->mapScreenCoordLocation_ = util::maplibre::LatLongToScreenCoordinate(
      {params.latitude, params.longitude});
   p->mapScale_ = std::pow(2.0, params.zoom) * mbgl::util::tileSize_D /
                  mbgl::util::DEGREES_MAX;
   p->mapBearingCos_ = cosf(params.bearing * common::kDegreesToRadians);
   p->mapBearingSin_ = sinf(params.bearing * common::kDegreesToRadians);
   p->halfWidth_     = params.width * 0.5f;
   p->halfHeight_    = params.height * 0.5f;

   p->hoverText_.clear();

   // Radar site ImGui windows shouldn't have padding
   ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 {0.0f, 0.0f});

   for (auto& radarSite : p->radarSites_)
   {
      p->RenderRadarSite(params, radarSite);
   }

   ImGui::PopStyleVar();

   SCWX_GL_CHECK_ERROR();
}

void RadarSiteLayer::Impl::RenderRadarSite(
   const QMapLibreGL::CustomLayerRenderParameters& params,
   std::shared_ptr<config::RadarSite>&             radarSite)
{
   const std::string windowName = fmt::format("radar-site-{}", radarSite->id());

   const auto screenCoordinates =
      (util::maplibre::LatLongToScreenCoordinate(
          {radarSite->latitude(), radarSite->longitude()}) -
       mapScreenCoordLocation_) *
      mapScale_;

   // Rotate text according to map rotation
   float rotatedX = screenCoordinates.x;
   float rotatedY = screenCoordinates.y;
   if (params.bearing != 0.0)
   {
      rotatedX = screenCoordinates.x * mapBearingCos_ -
                 screenCoordinates.y * mapBearingSin_;
      rotatedY = screenCoordinates.x * mapBearingSin_ +
                 screenCoordinates.y * mapBearingCos_;
   }

   // Convert screen to ImGui coordinates
   float x = rotatedX + halfWidth_;
   float y = params.height - (rotatedY + halfHeight_);

   // Setup window to hold text
   ImGui::SetNextWindowPos(
      ImVec2 {x, y}, ImGuiCond_Always, ImVec2 {0.5f, 0.5f});
   if (ImGui::Begin(windowName.c_str(),
                    nullptr,
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_AlwaysAutoResize))
   {
      // Render text
      if (ImGui::Button(radarSite->id().c_str()))
      {
         Q_EMIT self_->RadarSiteSelected(radarSite->id());
      }

      // Store hover text for mouse picking pass
      if (ImGui::IsItemHovered())
      {
         hoverText_ =
            fmt::format("{} ({})\n{}\n{}, {}",
                        radarSite->id(),
                        radarSite->type_name(),
                        radarSite->location_name(),
                        common::GetLatitudeString(radarSite->latitude()),
                        common::GetLongitudeString(radarSite->longitude()));
      }

      // End window
      ImGui::End();
   }
}

void RadarSiteLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");

   p->radarSites_.clear();
}

bool RadarSiteLayer::RunMousePicking(
   const QMapLibreGL::CustomLayerRenderParameters& /* params */,
   const QPointF& /* mouseLocalPos */,
   const QPointF& mouseGlobalPos,
   const glm::vec2& /* mouseCoords */,
   const common::Coordinate& /* mouseGeoCoords */)
{
   if (!p->hoverText_.empty())
   {
      util::tooltip::Show(p->hoverText_, mouseGlobalPos);
      return true;
   }

   return false;
}

} // namespace map
} // namespace qt
} // namespace scwx
