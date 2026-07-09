#include <scwx/qt/map/radar_site_layer.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/draw/geo_lines.hpp>
#include <scwx/qt/manager/radar_site_status_manager.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/settings/palette_settings.hpp>
#include <scwx/qt/settings/text_settings.hpp>
#include <scwx/qt/util/color.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/qt/util/tooltip.hpp>
#include <scwx/common/geographic.hpp>
#include <scwx/util/logger.hpp>

#include <ranges>

#include <boost/unordered/unordered_flat_map.hpp>
#include <imgui.h>
#include <mbgl/util/constants.hpp>

#include <QGuiApplication>

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::radar_site_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

namespace
{

class ImGuiStyleColorStack
{
public:
   ImGuiStyleColorStack(ImGuiCol      idx0,
                        const ImVec4& col0,
                        ImGuiCol      idx1,
                        const ImVec4& col1,
                        ImGuiCol      idx2,
                        const ImVec4& col2)
   {
      ImGui::PushStyleColor(idx0, col0);
      ImGui::PushStyleColor(idx1, col1);
      ImGui::PushStyleColor(idx2, col2);
   }

   ~ImGuiStyleColorStack() { ImGui::PopStyleColor(3); }

   ImGuiStyleColorStack(const ImGuiStyleColorStack&)            = delete;
   ImGuiStyleColorStack& operator=(const ImGuiStyleColorStack&) = delete;
};

class ImGuiIdScope
{
public:
   explicit ImGuiIdScope(const int id) { ImGui::PushID(id); }
   ~ImGuiIdScope() { ImGui::PopID(); }

   ImGuiIdScope(const ImGuiIdScope&)            = delete;
   ImGuiIdScope& operator=(const ImGuiIdScope&) = delete;
};

} // namespace

class RadarSiteLayer::Impl
{
public:
   explicit Impl(RadarSiteLayer*                               self,
                 const std::shared_ptr<render::RenderContext>& renderContext) :
       self_ {self}, geoLines_ {std::make_shared<draw::GeoLines>(renderContext)}
   {
   }
   ~Impl() = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   [[nodiscard]] bool
   IsVisible(const QMapLibre::CustomLayerRenderParameters& params) const;
   void
   UpdateMapTransform(const QMapLibre::CustomLayerRenderParameters& params);
   void UpdateButtonColors();
   void
   RenderRadarSiteButtons(const QMapLibre::CustomLayerRenderParameters& params);
   void RenderRadarSite(const QMapLibre::CustomLayerRenderParameters& params,
                        const std::shared_ptr<config::RadarSite>&     radarSite,
                        int siteIndex);
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

   std::shared_ptr<draw::GeoLines>                       geoLines_;
   std::array<std::shared_ptr<draw::GeoLineDrawItem>, 2> radarSiteLines_ {
      nullptr, nullptr};

   boost::unordered_flat_map<types::RadarSiteStatus,
                             std::tuple<ImVec4, ImVec4, ImVec4>>
      radarSiteStatusButtonColors_ {};
};

RadarSiteLayer::RadarSiteLayer(
   const std::shared_ptr<render::RenderContext>& renderContext) :
    DrawLayer(renderContext, "RadarSiteLayer"),
    p(std::make_unique<Impl>(this, renderContext))
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

bool RadarSiteLayer::Impl::IsVisible(
   const QMapLibre::CustomLayerRenderParameters& params) const
{
   auto mapDistance = util::maplibre::GetMapDistance(params);
   auto threshold   = units::length::kilometers<double>(
      settings::GeneralSettings::Instance().radar_site_threshold().GetValue());

   return threshold.value() == 0.0 || mapDistance <= threshold ||
          (threshold.value() < 0 && mapDistance >= -threshold);
}

void RadarSiteLayer::Impl::UpdateMapTransform(
   const QMapLibre::CustomLayerRenderParameters& params)
{
   mapScreenCoordLocation_ = util::maplibre::LatLongToScreenCoordinate(
      {params.latitude, params.longitude});
   mapScale_ = std::pow(2.0, params.zoom) * mbgl::util::tileSize_D /
               mbgl::util::DEGREES_MAX;
   mapBearingCos_ = cosf(params.bearing * common::kDegreesToRadians);
   mapBearingSin_ = sinf(params.bearing * common::kDegreesToRadians);
   halfWidth_     = params.width * 0.5f;
   halfHeight_    = params.height * 0.5f;
}

void RadarSiteLayer::Impl::UpdateButtonColors()
{
   auto& paletteSettings = settings::PaletteSettings::Instance();
   for (auto status : types::RadarSiteStatusIterator())
   {
      auto& statusPalette = paletteSettings.radar_site_status_palette(status);
      auto& buttonPalette = statusPalette.button();
      radarSiteStatusButtonColors_.insert_or_assign(
         status,
         std::tuple {
            util::color::ToImVec4(buttonPalette.button_color().GetValue()),
            util::color::ToImVec4(buttonPalette.hover_color().GetValue()),
            util::color::ToImVec4(buttonPalette.active_color().GetValue())});
   }
}

void RadarSiteLayer::Impl::RenderRadarSiteButtons(
   const QMapLibre::CustomLayerRenderParameters& params)
{
   if (radarSites_.empty())
   {
      return;
   }

   ImGui::SetNextWindowPos(ImVec2 {0.0f, 0.0f});
   ImGui::SetNextWindowSize(ImVec2 {static_cast<float>(params.width),
                                    static_cast<float>(params.height)});
   constexpr ImGuiWindowFlags kOverlayFlags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav |
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings;

   if (!ImGui::Begin("##radar-sites-overlay", nullptr, kOverlayFlags))
   {
      ImGui::End();
      return;
   }

   for (std::size_t i = 0; i < radarSites_.size(); ++i)
   {
      RenderRadarSite(params, radarSites_[i], static_cast<int>(i));
   }

   ImGui::End();
}

void RadarSiteLayer::Render(
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   p->hoverText_.clear();

   if (!p->IsVisible(params))
   {
      return;
   }

   p->UpdateMapTransform(params);

   ImGuiFrameStart(mapContext);
   ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 {0.0f, 0.0f});
   p->UpdateButtonColors();
   p->RenderRadarSiteButtons(params);
   ImGui::PopStyleVar();

   p->RenderRadarLine(mapContext);

   DrawLayer::RenderWithoutImGui(params);

   ImGuiFrameEnd();
}

void RadarSiteLayer::RenderVulkanOverlay(
   QRhiCommandBuffer*                            commandBuffer,
   render::RhiVulkanOverlayResources&            resources,
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   if (!p->IsVisible(params))
   {
      return;
   }

   p->UpdateMapTransform(params);
   p->RenderRadarLine(mapContext);
   RenderWithoutImGuiVulkan(commandBuffer, resources, params);
}

void RadarSiteLayer::RenderVulkanImGui(
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   (void) mapContext;

   p->hoverText_.clear();

   if (!p->IsVisible(params))
   {
      return;
   }

   p->UpdateMapTransform(params);

   ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 {0.0f, 0.0f});
   p->UpdateButtonColors();
   p->RenderRadarSiteButtons(params);
   ImGui::PopStyleVar();
}

void RadarSiteLayer::Impl::RenderRadarSite(
   const QMapLibre::CustomLayerRenderParameters& params,
   const std::shared_ptr<config::RadarSite>&     radarSite,
   const int                                     siteIndex)
{
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
   const float x = rotatedX + halfWidth_;
   const float y = params.height - (rotatedY + halfHeight_);

   static constexpr float kCullMargin = 48.0f;
   if (x < -kCullMargin || x > params.width + kCullMargin || y < -kCullMargin ||
       y > params.height + kCullMargin)
   {
      return;
   }

   const auto radarSiteStatus = radarSite->status();
   const auto colorIt = radarSiteStatusButtonColors_.find(radarSiteStatus);
   if (colorIt == radarSiteStatusButtonColors_.end())
   {
      return;
   }

   const char* const label        = radarSite->id().c_str();
   const ImVec2      labelSize    = ImGui::CalcTextSize(label);
   const ImVec2&     framePadding = ImGui::GetStyle().FramePadding;
   const ImVec2      buttonSize {labelSize.x + framePadding.x * 2.0f,
                            labelSize.y + framePadding.y * 2.0f};

   ImGui::SetCursorScreenPos(
      ImVec2 {x - buttonSize.x * 0.5f, y - buttonSize.y * 0.5f});
   ImGuiIdScope idScope {siteIndex};

   ImGuiStyleColorStack buttonColors {ImGuiCol_Button,
                                      std::get<0>(colorIt->second),
                                      ImGuiCol_ButtonHovered,
                                      std::get<1>(colorIt->second),
                                      ImGuiCol_ButtonActive,
                                      std::get<2>(colorIt->second)};

   if (ImGui::Button(label, buttonSize))
   {
      Q_EMIT self_->RadarSiteSelected(radarSite->id());
   }

   // Store hover text for mouse picking pass
   if (ImGui::GetCurrentContext() != nullptr &&
       settings::TextSettings::Instance()
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
   DrawLayer::Deinitialize();
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
