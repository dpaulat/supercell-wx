#include <scwx/qt/map/overlay_layer.hpp>
#include <scwx/qt/gl/draw/geo_icons.hpp>
#include <scwx/qt/gl/draw/icons.hpp>
#include <scwx/qt/gl/draw/rectangle.hpp>
#include <scwx/qt/manager/position_manager.hpp>
#include <scwx/qt/types/texture_types.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <chrono>

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#include <imgui.h>
#include <QGeoPositionInfo>
#include <QMouseEvent>

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
       geoIcons_ {std::make_shared<gl::draw::GeoIcons>(context)},
       icons_ {std::make_shared<gl::draw::Icons>(context)},
       cardinalPointIconName_ {
          types::GetTextureName(types::ImageTexture::CardinalPoint24)},
       compassIconName_ {types::GetTextureName(types::ImageTexture::Compass24)},
       locationIconName_ {
          types::GetTextureName(types::ImageTexture::Crosshairs24)}
   {
   }
   ~OverlayLayerImpl() = default;

   std::shared_ptr<manager::PositionManager> positionManager_ {
      manager::PositionManager::Instance()};
   QGeoPositionInfo currentPosition_ {};

   std::shared_ptr<gl::draw::Rectangle> activeBoxOuter_;
   std::shared_ptr<gl::draw::Rectangle> activeBoxInner_;
   std::shared_ptr<gl::draw::GeoIcons>  geoIcons_;
   std::shared_ptr<gl::draw::Icons>     icons_;

   const std::string&                         locationIconName_;
   std::shared_ptr<gl::draw::GeoIconDrawItem> locationIcon_ {};

   const std::string&                      cardinalPointIconName_;
   const std::string&                      compassIconName_;
   std::shared_ptr<gl::draw::IconDrawItem> compassIcon_ {};
   bool                                    compassIconDirty_ {false};
   double                                  lastBearing_ {0.0};

   double lastWidth_ {0.0};
   double lastHeight_ {0.0};

   std::string sweepTimeString_ {};
   bool        sweepTimeNeedsUpdate_ {true};
   bool        sweepTimePicked_ {false};
};

OverlayLayer::OverlayLayer(std::shared_ptr<MapContext> context) :
    DrawLayer(context), p(std::make_unique<OverlayLayerImpl>(context))
{
   AddDrawItem(p->activeBoxOuter_);
   AddDrawItem(p->activeBoxInner_);
   AddDrawItem(p->geoIcons_);
   AddDrawItem(p->icons_);

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

   p->currentPosition_ = p->positionManager_->position();
   auto coordinate     = p->currentPosition_.coordinate();

   // Geo Icons
   p->geoIcons_->StartIconSheets();
   p->geoIcons_->AddIconSheet(p->locationIconName_);
   p->geoIcons_->FinishIconSheets();

   p->geoIcons_->StartIcons();
   p->locationIcon_ = p->geoIcons_->AddIcon();
   gl::draw::GeoIcons::SetIconTexture(
      p->locationIcon_, p->locationIconName_, 0);
   gl::draw::GeoIcons::SetIconAngle(p->locationIcon_,
                                    units::angle::degrees<double> {45.0});
   gl::draw::GeoIcons::SetIconLocation(
      p->locationIcon_, coordinate.latitude(), coordinate.longitude());
   p->geoIcons_->FinishIcons();

   // Icons
   p->icons_->StartIconSheets();
   p->icons_->AddIconSheet(p->cardinalPointIconName_);
   p->icons_->AddIconSheet(p->compassIconName_);
   p->icons_->FinishIconSheets();

   p->icons_->StartIcons();
   p->compassIcon_ = p->icons_->AddIcon();
   gl::draw::Icons::SetIconTexture(
      p->compassIcon_, p->cardinalPointIconName_, 0);
   gl::draw::Icons::RegisterEventHandler(
      p->compassIcon_,
      [this](QEvent* ev)
      {
         switch (ev->type())
         {
         case QEvent::Type::Enter:
            gl::draw::Icons::SetIconModulate(
               p->compassIcon_,
               boost::gil::rgba32f_pixel_t {1.5f, 1.5f, 1.5f, 1.0f});
            p->compassIconDirty_ = true;
            break;

         case QEvent::Type::Leave:
            gl::draw::Icons::SetIconModulate(
               p->compassIcon_,
               boost::gil::rgba32f_pixel_t {1.0f, 1.0f, 1.0f, 1.0f});
            p->compassIconDirty_ = true;
            break;

         case QEvent::Type::MouseButtonPress:
         {
            QMouseEvent* mouseEvent = reinterpret_cast<QMouseEvent*>(ev);
            if (mouseEvent->buttons() == Qt::MouseButton::LeftButton &&
                p->lastBearing_ != 0.0)
            {
               auto map = context()->map().lock();
               if (map != nullptr)
               {
                  map->setBearing(0.0);
               }
               ev->accept();
            }
            break;
         }
         }
      });
   p->icons_->FinishIcons();

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
                 gl::draw::GeoIcons::SetIconLocation(p->locationIcon_,
                                                     coordinate.latitude(),
                                                     coordinate.longitude());
                 p->geoIcons_->FinishIcons();
                 Q_EMIT NeedsRendering();
              }
              p->currentPosition_ = position;
           });
}

void OverlayLayer::Render(
   const QMapLibreGL::CustomLayerRenderParameters& params)
{
   gl::OpenGLFunctions& gl               = context()->gl();
   auto                 radarProductView = context()->radar_product_view();
   auto&                settings         = context()->settings();
   const float          pixelRatio       = context()->pixel_ratio();

   context()->set_render_parameters(params);

   // Set OpenGL blend mode for transparency
   gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   p->sweepTimePicked_ = false;

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

   // Location Icon
   p->geoIcons_->SetVisible(p->currentPosition_.isValid() &&
                            p->positionManager_->IsLocationTracked());

   // Compass Icon
   if (params.width != p->lastWidth_ || params.height != p->lastHeight_)
   {
      gl::draw::Icons::SetIconLocation(
         p->compassIcon_, params.width - 24, params.height - 48);
      p->compassIconDirty_ = true;
   }
   if (params.bearing != p->lastBearing_)
   {
      if (params.bearing == 0.0)
      {
         gl::draw::Icons::SetIconTexture(
            p->compassIcon_, p->cardinalPointIconName_, 0);
         gl::draw::Icons::SetIconAngle(p->compassIcon_,
                                       units::angle::degrees<double> {0.0});
      }
      else
      {
         gl::draw::Icons::SetIconTexture(
            p->compassIcon_, p->compassIconName_, 0);
         gl::draw::Icons::SetIconAngle(
            p->compassIcon_,
            units::angle::degrees<double> {-45 - params.bearing});
      }
      p->compassIconDirty_ = true;
   }
   if (p->compassIconDirty_)
   {
      p->icons_->FinishIcons();
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

      if (radarProductView != nullptr && ImGui::IsWindowHovered())
      {
         // Show a detailed product description when the sweep time is hovered
         p->sweepTimePicked_ = true;

         auto fields = radarProductView->GetDescriptionFields();
         if (fields.empty())
         {
            ImGui::TextUnformatted(p->sweepTimeString_.c_str());
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
         ImGui::TextUnformatted(p->sweepTimeString_.c_str());
      }

      ImGui::End();
   }

   p->lastWidth_   = params.width;
   p->lastHeight_  = params.height;
   p->lastBearing_ = params.bearing;

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

   disconnect(p->positionManager_.get(),
              &manager::PositionManager::LocationTrackingChanged,
              this,
              nullptr);
   disconnect(p->positionManager_.get(),
              &manager::PositionManager::PositionUpdated,
              this,
              nullptr);

   p->locationIcon_ = nullptr;
}

bool OverlayLayer::RunMousePicking(
   const QMapLibreGL::CustomLayerRenderParameters& params,
   const QPointF&                                  mouseLocalPos,
   const QPointF&                                  mouseGlobalPos,
   const glm::vec2&                                mouseCoords,
   const common::Coordinate&                       mouseGeoCoords,
   std::shared_ptr<types::EventHandler>&           eventHandler)
{
   // If sweep time was picked, don't process additional items
   if (p->sweepTimePicked_)
   {
      return true;
   }

   return DrawLayer::RunMousePicking(params,
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

} // namespace map
} // namespace qt
} // namespace scwx
