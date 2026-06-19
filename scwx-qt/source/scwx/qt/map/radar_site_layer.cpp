#include <scwx/qt/map/radar_site_layer.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/gl/draw/geo_lines.hpp>
#include <scwx/qt/manager/radar_site_status_manager.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/settings/palette_settings.hpp>
#include <scwx/qt/settings/text_settings.hpp>
#include <scwx/qt/util/color.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/qt/util/tooltip.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/util/logger.hpp>

#include <boost/unordered/unordered_flat_map.hpp>
#include <imgui.h>
#include <mbgl/util/constants.hpp>

#include <QGuiApplication>

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::radar_site_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class RadarSiteLayer::Impl
{
public:
   explicit Impl(RadarSiteLayer*                       self,
                 const std::shared_ptr<gl::GlContext>& glContext) :
       self_ {self}, geoLines_ {std::make_shared<gl::draw::GeoLines>(glContext)}
   {
   }
   ~Impl() = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void RenderRadarSite(const QMapLibre::CustomLayerRenderParameters& params,
                        std::shared_ptr<config::RadarSite>& radarSite);
   void RenderRadarLine(const std::shared_ptr<MapContext>& mapContext);

   RadarSiteLayer* self_;

   std::vector<std::shared_ptr<config::RadarSite>> radarSites_ {};

   glm::vec2 mapScreenCoordLocation_ {};
   float     mapScale_ {1.0f};
   float     mapBearingCos_ {1.0f};
   float     mapBearingSin_ {0.0f};
   float     halfWidth_ {};
   float     halfHeight_ {};

   std::string hoverText_ {};

   std::shared_ptr<gl::draw::GeoLines>                       geoLines_;
   std::array<std::shared_ptr<gl::draw::GeoLineDrawItem>, 2> radarSiteLines_ {
      nullptr, nullptr};

   boost::unordered_flat_map<types::RadarSiteStatus,
                             std::tuple<ImVec4, ImVec4, ImVec4>>
      radarSiteStatusButtonColors_ {};
};

RadarSiteLayer::RadarSiteLayer(
   const std::shared_ptr<gl::GlContext>& glContext) :
    DrawLayer(glContext, "RadarSiteLayer"),
    p(std::make_unique<Impl>(this, glContext))
{
   connect(manager::RadarSiteStatusManager::Instance().get(),
           &manager::RadarSiteStatusManager::StatusUpdated,
           this,
           &GenericLayer::NeedsRendering);
}

RadarSiteLayer::~RadarSiteLayer() = default;

void RadarSiteLayer::Initialize(const std::shared_ptr<MapContext>& mapContext)
{
   logger_->debug("Initialize()");

   p->radarSites_ = config::RadarSite::GetAll();

   p->geoLines_->StartLines();
   p->radarSiteLines_[0] = p->geoLines_->AddLine();
   p->radarSiteLines_[1] = p->geoLines_->AddLine();
   p->geoLines_->FinishLines();

   static const boost::gil::rgba32f_pixel_t color0 {0.0f, 0.0f, 0.0f, 1.0f};
   static const boost::gil::rgba32f_pixel_t color1 {1.0f, 1.0f, 1.0f, 1.0f};
   static const float                       width = 1;
   p->geoLines_->SetLineModulate(p->radarSiteLines_[0], color0);
   p->geoLines_->SetLineWidth(p->radarSiteLines_[0], width + 2);

   p->geoLines_->SetLineModulate(p->radarSiteLines_[1], color1);
   p->geoLines_->SetLineWidth(p->radarSiteLines_[1], width);

   AddDrawItem(p->geoLines_);
   p->geoLines_->set_thresholded(false);

   DrawLayer::Initialize(mapContext);
}

void RadarSiteLayer::Render(
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   p->hoverText_.clear();

   auto mapDistance = util::maplibre::GetMapDistance(params);
   auto threshold   = units::length::kilometers<double>(
      settings::GeneralSettings::Instance().radar_site_threshold().GetValue());

   if (!(threshold.value() == 0.0 || mapDistance <= threshold ||
         (threshold.value() < 0 && mapDistance >= -threshold)))
   {
      return;
   }

   // Update map screen coordinate and scale information
   p->mapScreenCoordLocation_ = util::maplibre::LatLongToScreenCoordinate(
      {params.latitude, params.longitude});
   p->mapScale_ = std::pow(2.0, params.zoom) * mbgl::util::tileSize_D /
                  mbgl::util::DEGREES_MAX;
   p->mapBearingCos_ = cosf(params.bearing * common::kDegreesToRadians);
   p->mapBearingSin_ = sinf(params.bearing * common::kDegreesToRadians);
   p->halfWidth_     = params.width * 0.5f;
   p->halfHeight_    = params.height * 0.5f;

   ImGuiFrameStart(mapContext);
   // Radar site ImGui windows shouldn't have padding
   ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 {0.0f, 0.0f});

   // Update Radar Site button colors
   auto& paletteSettings = settings::PaletteSettings::Instance();
   for (auto status : types::RadarSiteStatusIterator())
   {
      auto& statusPalette = paletteSettings.radar_site_status_palette(status);
      auto& buttonPalette = statusPalette.button();
      p->radarSiteStatusButtonColors_.insert_or_assign(
         status,
         std::tuple {
            util::color::ToImVec4(buttonPalette.button_color().GetValue()),
            util::color::ToImVec4(buttonPalette.hover_color().GetValue()),
            util::color::ToImVec4(buttonPalette.active_color().GetValue())});
   }

   // Render Radar Sites
   for (auto& radarSite : p->radarSites_)
   {
      p->RenderRadarSite(params, radarSite);
   }

   ImGui::PopStyleVar();

   p->RenderRadarLine(mapContext);

   DrawLayer::RenderWithoutImGui(params);

   ImGuiFrameEnd();
   SCWX_GL_CHECK_ERROR();
}

void RadarSiteLayer::Impl::RenderRadarSite(
   const QMapLibre::CustomLayerRenderParameters& params,
   std::shared_ptr<config::RadarSite>&           radarSite)
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
      static constexpr int kPushStyleCount = 3;
      auto                 radarSiteStatus = radarSite->status();

      ImGui::PushStyleColor(
         ImGuiCol_Button,
         std::get<0>(radarSiteStatusButtonColors_.at(radarSiteStatus)));
      ImGui::PushStyleColor(
         ImGuiCol_ButtonHovered,
         std::get<1>(radarSiteStatusButtonColors_.at(radarSiteStatus)));
      ImGui::PushStyleColor(
         ImGuiCol_ButtonActive,
         std::get<2>(radarSiteStatusButtonColors_.at(radarSiteStatus)));

      // Render text
      if (ImGui::Button(radarSite->id().c_str()))
      {
         Q_EMIT self_->RadarSiteSelected(radarSite->id());
         self_->ImGuiSelectContext();
      }

      // Store hover text for mouse picking pass
      if (settings::TextSettings::Instance()
             .radar_site_hover_text_enabled()
             .GetValue() &&
          ImGui::IsItemHovered())
      {
         hoverText_ =
            fmt::format("{} ({})\n{}\n{}, {}",
                        radarSite->id(),
                        radarSite->type_name(),
                        radarSite->location_name(),
                        common::GetLatitudeString(radarSite->latitude()),
                        common::GetLongitudeString(radarSite->longitude()));
      }

      ImGui::PopStyleColor(kPushStyleCount);

      // End window
      ImGui::End();
   }
}

void RadarSiteLayer::Impl::RenderRadarLine(
   const std::shared_ptr<MapContext>& mapContext)
{
   if ((QGuiApplication::keyboardModifiers() &
        Qt::KeyboardModifier::ShiftModifier) &&
       mapContext->radar_site() != nullptr)
   {
      const auto&  mouseCoord     = mapContext->mouse_coordinate();
      const double radarLatitude  = mapContext->radar_site()->latitude();
      const double radarLongitude = mapContext->radar_site()->longitude();

      geoLines_->SetLineLocation(radarSiteLines_[0],
                                 static_cast<float>(mouseCoord.latitude_),
                                 static_cast<float>(mouseCoord.longitude_),
                                 static_cast<float>(radarLatitude),
                                 static_cast<float>(radarLongitude));
      geoLines_->SetLineVisible(radarSiteLines_[0], true);

      geoLines_->SetLineLocation(radarSiteLines_[1],
                                 static_cast<float>(mouseCoord.latitude_),
                                 static_cast<float>(mouseCoord.longitude_),
                                 static_cast<float>(radarLatitude),
                                 static_cast<float>(radarLongitude));
      geoLines_->SetLineVisible(radarSiteLines_[1], true);
   }
   else
   {
      geoLines_->SetLineVisible(radarSiteLines_[0], false);
      geoLines_->SetLineVisible(radarSiteLines_[1], false);
   }
}

void RadarSiteLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");

   p->radarSites_.clear();
}

bool RadarSiteLayer::RunMousePicking(
   const std::shared_ptr<MapContext>& /* mapContext */,
   const QMapLibre::CustomLayerRenderParameters& /* params */,
   const QPointF& /* mouseLocalPos */,
   const QPointF& mouseGlobalPos,
   const glm::vec2& /* mouseCoords */,
   const common::Coordinate& /* mouseGeoCoords */,
   std::shared_ptr<types::EventHandler>& /* eventHandler */)
{
   if (!p->hoverText_.empty())
   {
      util::tooltip::Show(p->hoverText_, mouseGlobalPos);
      return true;
   }

   return false;
}

} // namespace scwx::qt::map
