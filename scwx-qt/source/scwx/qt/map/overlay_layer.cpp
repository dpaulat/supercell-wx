#include <scwx/common/characters.hpp>
#include <scwx/qt/draw/geo_icons.hpp>
#include <scwx/qt/draw/icons.hpp>
#include <scwx/qt/draw/rectangle.hpp>
#include <scwx/qt/manager/font_manager.hpp>
#include <scwx/qt/manager/position_manager.hpp>
#include <scwx/qt/manager/resource_manager.hpp>
#include <scwx/qt/map/map_settings.hpp>
#include <scwx/qt/map/overlay_layer.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/types/texture_types.hpp>
#include <scwx/qt/view/overlay_product_view.hpp>
#include <scwx/qt/view/radar_product_view.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#include <imgui.h>
#include <QGeoPositionInfo>
#include <QGuiApplication>
#include <QMouseEvent>

#include <cmath>

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::overlay_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class OverlayLayer::Impl
{
public:
   explicit Impl(OverlayLayer*                                 self,
                 const std::shared_ptr<render::RenderContext>& renderContext) :
       self_ {self},
       activeBoxOuter_ {std::make_shared<draw::Rectangle>(renderContext)},
       activeBoxInner_ {std::make_shared<draw::Rectangle>(renderContext)},
       geoIcons_ {std::make_shared<draw::GeoIcons>(renderContext)},
       icons_ {std::make_shared<draw::Icons>(renderContext)},
       renderMutex_ {}
   {
      auto& generalSettings = settings::GeneralSettings::Instance();

      clockFormatCallbackUuid_ =
         generalSettings.clock_format().RegisterValueChangedCallback(
            [this](const std::string&)
            {
               sweepTimeNeedsUpdate_ = true;
               Q_EMIT self_->NeedsRendering();
            });
      defaultTimeZoneCallbackUuid_ =
         generalSettings.default_time_zone().RegisterValueChangedCallback(
            [this](const std::string&)
            {
               sweepTimeNeedsUpdate_ = true;
               Q_EMIT self_->NeedsRendering();
            });
      showMapAttributionCallbackUuid_ =
         generalSettings.show_map_attribution().RegisterValueChangedCallback(
            [this](const bool&) { Q_EMIT self_->NeedsRendering(); });
      showMapCenterCallbackUuid_ =
         generalSettings.show_map_center().RegisterValueChangedCallback(
            [this](const bool&) { Q_EMIT self_->NeedsRendering(); });
      showMapLogoCallbackUuid_ =
         generalSettings.show_map_logo().RegisterValueChangedCallback(
            [this](const bool&) { Q_EMIT self_->NeedsRendering(); });
   }

   ~Impl()
   {
      auto& generalSettings = settings::GeneralSettings::Instance();

      generalSettings.clock_format().UnregisterValueChangedCallback(
         clockFormatCallbackUuid_);
      generalSettings.default_time_zone().UnregisterValueChangedCallback(
         defaultTimeZoneCallbackUuid_);
      generalSettings.show_map_attribution().UnregisterValueChangedCallback(
         showMapAttributionCallbackUuid_);
      generalSettings.show_map_center().UnregisterValueChangedCallback(
         showMapCenterCallbackUuid_);
      generalSettings.show_map_logo().UnregisterValueChangedCallback(
         showMapLogoCallbackUuid_);
   }

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void RenderProductName(const std::shared_ptr<MapContext>& mapContext);
   void
        RenderProductDetails(const std::shared_ptr<MapContext>& mapContext,
                             const QMapLibre::CustomLayerRenderParameters& params);
   void RenderAttribution(const std::shared_ptr<MapContext>& mapContext,
                          const QMapLibre::CustomLayerRenderParameters& params);
   void SetupScreenIcons(const std::shared_ptr<MapContext>& mapContext);
   void UpdateScreenIcons(const std::shared_ptr<MapContext>& mapContext,
                          const QMapLibre::CustomLayerRenderParameters& params);
   void UpdateImGuiState(const QMapLibre::CustomLayerRenderParameters& params);
   void UpdateSweepTimeString(const std::shared_ptr<MapContext>& mapContext);
   void
        RenderImGuiOverlay(const std::shared_ptr<MapContext>&            mapContext,
                           const QMapLibre::CustomLayerRenderParameters& params);
   void UpdateDrawItems(const std::shared_ptr<MapContext>& mapContext,
                        const QMapLibre::CustomLayerRenderParameters& params);

   void SetupGeoIcons();
   void SetCursorLocation(common::Coordinate coordinate);

   OverlayLayer* self_;

   boost::uuids::uuid clockFormatCallbackUuid_;
   boost::uuids::uuid defaultTimeZoneCallbackUuid_;
   boost::uuids::uuid showMapAttributionCallbackUuid_;
   boost::uuids::uuid showMapCenterCallbackUuid_;
   boost::uuids::uuid showMapLogoCallbackUuid_;

   std::shared_ptr<manager::PositionManager> positionManager_ {
      manager::PositionManager::Instance()};
   QGeoPositionInfo currentPosition_ {};

   std::shared_ptr<draw::Rectangle> activeBoxOuter_;
   std::shared_ptr<draw::Rectangle> activeBoxInner_;
   std::shared_ptr<draw::GeoIcons>  geoIcons_;
   std::shared_ptr<draw::Icons>     icons_;

   const std::string& locationIconName_ {
      types::GetTextureName(types::ImageTexture::Crosshairs24)};
   std::shared_ptr<draw::GeoIconDrawItem> locationIcon_ {};

   std::shared_ptr<draw::GeoIconDrawItem> cursorIcon_ {};

   const std::string& cardinalPointIconName_ {
      types::GetTextureName(types::ImageTexture::CardinalPoint24)};
   const std::string& compassIconName_ {
      types::GetTextureName(types::ImageTexture::Compass24)};
   std::string cursorIconName_ {
      types::GetTextureName(types::ImageTexture::Dot3)};
   const std::string& mapCenterIconName_ {
      types::GetTextureName(types::ImageTexture::Cursor17)};

   std::shared_ptr<boost::gil::rgba8_image_t> cursorIconImage_ {nullptr};

   const std::unordered_map<MapProvider, const std::string&> mapProviderLogos_ {
      {MapProvider::Mapbox,
       types::GetTextureName(types::ImageTexture::MapboxLogo)},
      {MapProvider::MapTiler,
       types::GetTextureName(types::ImageTexture::MapTilerLogo)},
      {MapProvider::OpenFreeMap,
       types::GetTextureName(types::ImageTexture::OpenFreeMapLogo)},
   };

   std::shared_ptr<draw::IconDrawItem> compassIcon_ {};
   std::shared_ptr<draw::IconDrawItem> mapCenterIcon_ {};
   double                              lastBearing_ {0.0};

   std::shared_ptr<draw::IconDrawItem> mapLogoIcon_ {};

   bool     firstRender_ {true};
   double   lastWidth_ {0.0};
   double   lastHeight_ {0.0};
   float    lastFontSize_ {0.0f};
   QMargins lastColorTableMargins_ {};

   double                             cursorScale_ {1};
   boost::signals2::scoped_connection cursorScaleConnection_;

   std::mutex renderMutex_;

   std::string sweepTimeString_ {};
   bool        sweepTimeNeedsUpdate_ {true};
   bool        sweepTimePicked_ {false};
};

OverlayLayer::OverlayLayer(
   const std::shared_ptr<render::RenderContext>& renderContext) :
    DrawLayer(renderContext, "OverlayLayer"),
    p(std::make_unique<Impl>(this, renderContext))
{
   AddDrawItem(p->activeBoxOuter_);
   AddDrawItem(p->activeBoxInner_);
   AddDrawItem(p->geoIcons_);
   AddDrawItem(p->icons_);

   p->activeBoxOuter_->SetPosition(0.0f, 0.0f);
}

OverlayLayer::~OverlayLayer()
{
   p->cursorScaleConnection_.disconnect();
}

void OverlayLayer::Impl::SetCursorLocation(common::Coordinate coordinate)
{
   geoIcons_->SetIconLocation(
      cursorIcon_, coordinate.latitude_, coordinate.longitude_);
}

void OverlayLayer::Impl::SetupGeoIcons()
{
   const std::unique_lock lock {renderMutex_};

   auto& generalSettings = settings::GeneralSettings::Instance();
   cursorScale_          = generalSettings.cursor_icon_scale().GetValue();

   const std::string& texturePath =
      types::GetTexturePath(types::ImageTexture::Dot3);
   cursorIconName_ = fmt::format(
      "{}x{}", types::GetTextureName(types::ImageTexture::Dot3), cursorScale_);
   cursorIconImage_ = manager::ResourceManager::LoadImageResource(
      texturePath, cursorIconName_, cursorScale_);
   manager::ResourceManager::BuildAtlas();

   auto coordinate = currentPosition_.coordinate();
   geoIcons_->StartIconSheets();
   geoIcons_->AddIconSheet(cursorIconName_);
   geoIcons_->AddIconSheet(locationIconName_);
   geoIcons_->FinishIconSheets();

   geoIcons_->StartIcons();

   cursorIcon_ = geoIcons_->AddIcon();
   geoIcons_->SetIconTexture(cursorIcon_, cursorIconName_, 0);

   locationIcon_ = geoIcons_->AddIcon();
   geoIcons_->SetIconTexture(locationIcon_, locationIconName_, 0);
   geoIcons_->SetIconLocation(
      locationIcon_, coordinate.latitude(), coordinate.longitude());

   geoIcons_->FinishIcons();
}

void OverlayLayer::Impl::SetupScreenIcons(
   const std::shared_ptr<MapContext>& mapContext)
{
   icons_->StartIconSheets();
   icons_->AddIconSheet(cardinalPointIconName_);
   icons_->AddIconSheet(compassIconName_);
   icons_->AddIconSheet(mapCenterIconName_);
   for (const auto& logoEntry : mapProviderLogos_)
   {
      icons_->AddIconSheet(logoEntry.second)->SetAnchor(0.0f, 1.0f);
   }
   icons_->FinishIconSheets();

   icons_->StartIcons();
   compassIcon_ = icons_->AddIcon();
   icons_->SetIconTexture(compassIcon_, cardinalPointIconName_, 0);
   draw::Icons::RegisterEventHandler(
      compassIcon_,
      [this, mapContext](QEvent* ev)
      {
         switch (ev->type())
         {
         case QEvent::Type::Enter:
            icons_->SetIconModulate(
               compassIcon_,
               boost::gil::rgba32f_pixel_t {1.5f, 1.5f, 1.5f, 1.0f});
            break;

         case QEvent::Type::Leave:
            icons_->SetIconModulate(
               compassIcon_,
               boost::gil::rgba32f_pixel_t {1.0f, 1.0f, 1.0f, 1.0f});
            break;

         case QEvent::Type::MouseButtonRelease:
         {
            QMouseEvent* mouseEvent = reinterpret_cast<QMouseEvent*>(ev);
            if (mouseEvent->button() == Qt::MouseButton::LeftButton)
            {
               auto map = mapContext->map().lock();
               if (map != nullptr)
               {
                  map->setBearing(0.0);
               }
               ev->accept();
            }
            break;
         }

         default:
            break;
         }
      });

   mapCenterIcon_ = icons_->AddIcon();
   icons_->SetIconTexture(mapCenterIcon_, mapCenterIconName_, 0);

   mapLogoIcon_       = icons_->AddIcon();
   const auto& logoIt = mapProviderLogos_.find(mapContext->map_provider());
   if (logoIt != mapProviderLogos_.cend())
   {
      icons_->SetIconTexture(mapLogoIcon_, logoIt->second, 0);
   }

   icons_->FinishIcons();
}

void OverlayLayer::Impl::UpdateScreenIcons(
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   auto& generalSettings = settings::GeneralSettings::Instance();

   static constexpr int kCompassMargin = 8;

   if (params.width != lastWidth_ || params.height != lastHeight_ ||
       firstRender_)
   {
      icons_->SetIconLocation(
         compassIcon_, params.width - 24 - kCompassMargin, kCompassMargin);
   }

   if (params.bearing != lastBearing_)
   {
      if (params.bearing == 0.0)
      {
         icons_->SetIconTexture(compassIcon_, cardinalPointIconName_, 0);
         icons_->SetIconAngle(compassIcon_,
                              units::angle::degrees<double> {0.0});
      }
      else
      {
         icons_->SetIconTexture(compassIcon_, compassIconName_, 0);
         icons_->SetIconAngle(
            compassIcon_, units::angle::degrees<double> {-45 - params.bearing});
      }
   }

   if (params.width != lastWidth_ || params.height != lastHeight_)
   {
      static constexpr double xPosition = 0.5;
      static constexpr double yPosition = 0.5;

      icons_->SetIconLocation(
         mapCenterIcon_, params.width * xPosition, params.height * yPosition);
   }
   icons_->SetIconVisible(mapCenterIcon_,
                          generalSettings.show_map_center().GetValue());

   const QMargins colorTableMargins = mapContext->color_table_margins();
   if (colorTableMargins != lastColorTableMargins_ || firstRender_)
   {
      static constexpr int xOffset = 10;
      static constexpr int yOffset = 10;

      icons_->SetIconLocation(mapLogoIcon_,
                              colorTableMargins.left() + xOffset,
                              colorTableMargins.bottom() + yOffset);
   }
   icons_->SetIconVisible(mapLogoIcon_,
                          generalSettings.show_map_logo().GetValue());
}

void OverlayLayer::Initialize(const std::shared_ptr<MapContext>& mapContext)
{
   logger_->debug("Initialize()");

   DrawLayer::Initialize(mapContext);

   auto radarProductView = mapContext->radar_product_view();

   if (radarProductView != nullptr)
   {
      connect(radarProductView.get(),
              &view::RadarProductView::SweepComputed,
              this,
              &OverlayLayer::UpdateSweepTimeNextFrame);
   }

   p->currentPosition_ = p->positionManager_->position();

   auto& generalSettings = settings::GeneralSettings::Instance();
   p->SetupGeoIcons();
   p->cursorScaleConnection_ =
      generalSettings.cursor_icon_scale().changed_signal().connect(
         [this](auto&&...)
         {
            p->SetupGeoIcons();
            Q_EMIT NeedsRendering();
         });

   p->SetupScreenIcons(mapContext);

   connect(p->positionManager_.get(),
           &manager::PositionManager::LocationTrackingChanged,
           this,
           [this]() { Q_EMIT NeedsRendering(); });
   connect(p->positionManager_.get(),
           &manager::PositionManager::PositionUpdated,
           this,
           [this](const QGeoPositionInfo& position)
           {
              auto coordinate = position.coordinate();
              if (position.isValid() &&
                  p->currentPosition_.coordinate() != coordinate)
              {
                 if (p->locationIcon_ != nullptr)
                 {
                    p->geoIcons_->SetIconLocation(p->locationIcon_,
                                                  coordinate.latitude(),
                                                  coordinate.longitude());
                 }
                 Q_EMIT NeedsRendering();
              }
              p->currentPosition_ = position;
           });
}

void OverlayLayer::Impl::UpdateDrawItems(
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   auto&       settings   = mapContext->settings();
   const float pixelRatio = mapContext->pixel_ratio();

   // Active Box
   activeBoxOuter_->SetVisible(settings.isActive_ &&
                               !mapContext->screen_capture());
   activeBoxInner_->SetVisible(settings.isActive_ &&
                               !mapContext->screen_capture());
   if (settings.isActive_)
   {
      activeBoxOuter_->SetSize(params.width, params.height);
      activeBoxInner_->SetSize(params.width - (2.0f * pixelRatio),
                               params.height - (2.0f * pixelRatio));

      activeBoxInner_->SetPosition(1.0f * pixelRatio, 1.0f * pixelRatio);

      activeBoxOuter_->SetBorder(1.0f * pixelRatio, {0, 0, 0, 255});
      activeBoxInner_->SetBorder(1.0f * pixelRatio, {255, 255, 255, 255});
   }

   auto& generalSettings = settings::GeneralSettings::Instance();

   // Cursor Icon
   const bool cursorIconVisible =
      generalSettings.cursor_icon_always_on().GetValue() ||
      (QGuiApplication::keyboardModifiers() &
       Qt::KeyboardModifier::ControlModifier);
   geoIcons_->SetIconVisible(cursorIcon_, cursorIconVisible);
   if (cursorIconVisible)
   {
      SetCursorLocation(mapContext->mouse_coordinate());
   }

   // Location Icon
   geoIcons_->SetIconVisible(locationIcon_,
                             currentPosition_.isValid() &&
                                positionManager_->IsLocationTracked());

   UpdateScreenIcons(mapContext, params);

   const QMargins colorTableMargins = mapContext->color_table_margins();
   lastColorTableMargins_           = colorTableMargins;
}

void OverlayLayer::Render(const std::shared_ptr<MapContext>& mapContext,
                          const QMapLibre::CustomLayerRenderParameters& params)
{
   const std::unique_lock lock {p->renderMutex_};

   auto radarProductView = mapContext->radar_product_view();

   ImGuiFrameStart(mapContext);

   p->sweepTimePicked_ = false;

   if (radarProductView != nullptr)
   {
      scwx::util::ClockFormat const clockFormat = scwx::util::GetClockFormat(
         settings::GeneralSettings::Instance().clock_format().GetValue());

      auto radarProductManager = radarProductView->radar_product_manager();

      const scwx::util::time_zone* currentZone =
         (radarProductManager != nullptr) ?
            radarProductManager->default_time_zone() :
            nullptr;

      p->sweepTimeString_ = scwx::util::TimeString(
         radarProductView->sweep_time(), clockFormat, currentZone, false);
      p->sweepTimeNeedsUpdate_ = false;
   }
   else
   {
      // No radar data: show timeline / overlay time so placefiles, alerts, and
      // animation stay correlated with the selected time.
      auto overlayView = mapContext->overlay_product_view();
      if (overlayView != nullptr)
      {
         scwx::util::ClockFormat const clockFormat = scwx::util::GetClockFormat(
            settings::GeneralSettings::Instance().clock_format().GetValue());

         const std::chrono::system_clock::time_point selectedTime =
            overlayView->selected_time();
         if (selectedTime == std::chrono::system_clock::time_point {})
         {
            p->sweepTimeString_ = "Live";
         }
         else
         {
            p->sweepTimeString_ =
               scwx::util::TimeString(selectedTime,
                                      clockFormat,
                                      scwx::util::time::current_time_zone(),
                                      false);
         }
      }
      else
      {
         p->sweepTimeString_.clear();
      }
   }

   p->UpdateDrawItems(mapContext, params);

   p->RenderProductName(mapContext);
   p->RenderProductDetails(mapContext, params);

   DrawLayer::RenderWithoutImGui(params);

   p->RenderAttribution(mapContext, params);

   p->UpdateImGuiState(params);

   ImGuiFrameEnd();
}

void OverlayLayer::Impl::UpdateImGuiState(
   const QMapLibre::CustomLayerRenderParameters& params)
{
   firstRender_  = false;
   lastWidth_    = params.width;
   lastHeight_   = params.height;
   lastBearing_  = params.bearing;
   lastFontSize_ = ImGui::GetFontSize();
}

void OverlayLayer::Impl::RenderImGuiOverlay(
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   RenderProductName(mapContext);
   RenderProductDetails(mapContext, params);
   RenderAttribution(mapContext, params);
   UpdateImGuiState(params);
}

void OverlayLayer::Impl::UpdateSweepTimeString(
   const std::shared_ptr<MapContext>& mapContext)
{
   auto radarProductView = mapContext->radar_product_view();

   if (radarProductView != nullptr)
   {
      scwx::util::ClockFormat const clockFormat = scwx::util::GetClockFormat(
         settings::GeneralSettings::Instance().clock_format().GetValue());

      auto radarProductManager = radarProductView->radar_product_manager();

      const scwx::util::time_zone* currentZone =
         (radarProductManager != nullptr) ?
            radarProductManager->default_time_zone() :
            nullptr;

      sweepTimeString_ = scwx::util::TimeString(
         radarProductView->sweep_time(), clockFormat, currentZone, false);
      sweepTimeNeedsUpdate_ = false;
   }
   else
   {
      auto overlayView = mapContext->overlay_product_view();
      if (overlayView != nullptr)
      {
         scwx::util::ClockFormat const clockFormat = scwx::util::GetClockFormat(
            settings::GeneralSettings::Instance().clock_format().GetValue());

         const std::chrono::system_clock::time_point selectedTime =
            overlayView->selected_time();
         if (selectedTime == std::chrono::system_clock::time_point {})
         {
            sweepTimeString_ = "Live";
         }
         else
         {
            sweepTimeString_ =
               scwx::util::TimeString(selectedTime,
                                      clockFormat,
                                      scwx::util::time::current_time_zone(),
                                      false);
         }
      }
      else
      {
         sweepTimeString_.clear();
      }
   }
}

void OverlayLayer::RenderVulkanOverlay(
   QRhiCommandBuffer*                            commandBuffer,
   render::RhiVulkanOverlayResources&            resources,
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   const std::unique_lock lock {p->renderMutex_};

   p->UpdateDrawItems(mapContext, params);
   p->UpdateImGuiState(params);

   DrawLayer::RenderVulkanOverlay(commandBuffer, resources, mapContext, params);
}

void OverlayLayer::RenderVulkanImGui(
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   const std::unique_lock lock {p->renderMutex_};

   p->sweepTimePicked_ = false;
   p->UpdateSweepTimeString(mapContext);
   p->RenderImGuiOverlay(mapContext, params);
}

void OverlayLayer::Impl::RenderProductName(
   const std::shared_ptr<MapContext>& mapContext)
{
   auto radarProductView = mapContext->radar_product_view();

   if (radarProductView != nullptr)
   {
      // Render product name
      const std::string productName = radarProductView->GetRadarProductName();
      const std::optional<float> elevation = radarProductView->elevation();

      if (productName.length() > 0 && !productName.starts_with('?'))
      {
         ImGui::SetNextWindowPos(ImVec2 {0.0f, 0.0f});
         ImGui::Begin("Product Name",
                      nullptr,
                      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_AlwaysAutoResize);

         if (elevation.has_value())
         {
            const std::string elevationString =
               (QString::number(*elevation, 'f', 1) +
                common::Characters::DEGREE)
                  .toStdString();
            ImGui::TextUnformatted(
               fmt::format("{} ({})", productName, elevationString).c_str());
         }
         else
         {
            ImGui::TextUnformatted(productName.c_str());
         }

         ImGui::End();
      }
   }
}

void OverlayLayer::Impl::RenderProductDetails(
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   auto radarProductView = mapContext->radar_product_view();

   ImGui::SetNextWindowPos(ImVec2 {static_cast<float>(params.width), 0.0f},
                           ImGuiCond_Always,
                           ImVec2 {1.0f, 0.0f});

   bool                          productNotAvailable = false;
   types::RadarProductLoadStatus newLoadStatus =
      types::RadarProductLoadStatus::ProductNotLoaded;

   if (radarProductView != nullptr)
   {
      newLoadStatus = radarProductView->load_status();

      switch (newLoadStatus)
      {
      case types::RadarProductLoadStatus::ProductNotAvailable:
         productNotAvailable = true;
         break;

      default:
         productNotAvailable = false;
      }
   }

   if (productNotAvailable)
   {
      ImGui::Begin("Product Not Available",
                   nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                      ImGuiWindowFlags_AlwaysAutoResize);

      ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
      ImGui::TextUnformatted("NO DATA AVAILABLE");
      ImGui::PopStyleColor();
      if (ImGui::BeginItemTooltip())
      {
         static constexpr float  kFontSizeFactor_  = 20.0f;
         static constexpr double kMaxWidthPercent_ = 0.8;

         ImGui::PushTextWrapPos(
            std::min(ImGui::GetFontSize() * kFontSizeFactor_,
                     static_cast<float>(params.width * kMaxWidthPercent_)));
         ImGui::TextUnformatted(
            "No data found for the selected product and time. Please select a "
            "different product, or update your time selection.");
         ImGui::PopTextWrapPos();
         ImGui::EndTooltip();
      }

      ImGui::End();
   }
   else if (sweepTimeString_.length() > 0)
   {
      // Render time
      ImGui::Begin("Product Details",
                   nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                      ImGuiWindowFlags_AlwaysAutoResize);

      if (radarProductView != nullptr && ImGui::IsWindowHovered())
      {
         // Show a detailed product description when the sweep time is hovered
         sweepTimePicked_ = true;

         auto fields = radarProductView->GetDescriptionFields();
         if (fields.empty())
         {
            ImGui::TextUnformatted(sweepTimeString_.c_str());
         }
         else
         {
            if (ImGui::BeginTable("Description Fields", 2))
            {
               for (auto& field : fields)
               {
                  ImGui::TableNextRow();
                  ImGui::TableNextColumn();
                  ImGui::TextUnformatted(field.first.c_str());
                  ImGui::TableNextColumn();
                  ImGui::TextUnformatted(field.second.c_str());
               }
               ImGui::EndTable();
            }
         }
      }
      else
      {
         ImGui::TextUnformatted(sweepTimeString_.c_str());
      }

      ImGui::End();
   }
}

void OverlayLayer::Impl::RenderAttribution(
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   const QMargins colorTableMargins = mapContext->color_table_margins();
   auto&          generalSettings   = settings::GeneralSettings::Instance();

   auto mapCopyrights = mapContext->map_copyrights();
   if (mapCopyrights.length() > 0 &&
       generalSettings.show_map_attribution().GetValue())
   {
      auto attributionFont = manager::FontManager::Instance().GetImGuiFont(
         types::FontCategory::Attribution);

      static constexpr float kWindowBgAlpha_  = 0.5f;
      static constexpr float kWindowPaddingX_ = 3.0f;
      static constexpr float kWindowPaddingY_ = 2.0f;

      ImGui::SetNextWindowPos(
         ImVec2 {
            static_cast<float>(params.width),
            static_cast<float>(params.height - colorTableMargins.bottom())},
         ImGuiCond_Always,
         ImVec2 {1.0f, 1.0f});
      ImGui::SetNextWindowBgAlpha(kWindowBgAlpha_);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                          ImVec2 {kWindowPaddingX_, kWindowPaddingY_});
      ImGui::PushFont(attributionFont.first->font(),
                      attributionFont.second.value());
      ImGui::Begin("Attribution",
                   nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                      ImGuiWindowFlags_AlwaysAutoResize);
      ImGui::TextUnformatted(mapCopyrights.c_str());
      ImGui::End();
      ImGui::PopFont();
      ImGui::PopStyleVar();
   }
}

void OverlayLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");

   DrawLayer::Deinitialize();

   disconnect(this);

   p->locationIcon_ = nullptr;
}

bool OverlayLayer::RunMousePicking(
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params,
   const QPointF&                                mouseLocalPos,
   const QPointF&                                mouseGlobalPos,
   const glm::vec2&                              mouseCoords,
   const common::Coordinate&                     mouseGeoCoords,
   std::shared_ptr<types::EventHandler>&         eventHandler)
{
   // If sweep time was picked, don't process additional items
   if (p->sweepTimePicked_)
   {
      return true;
   }

   return DrawLayer::RunMousePicking(mapContext,
                                     params,
                                     mouseLocalPos,
                                     mouseGlobalPos,
                                     mouseCoords,
                                     mouseGeoCoords,
                                     eventHandler);
}

void OverlayLayer::UpdateSweepTimeNextFrame()
{
   p->sweepTimeNeedsUpdate_ = true;
}

} // namespace scwx::qt::map
