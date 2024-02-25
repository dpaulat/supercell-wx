#include <scwx/qt/map/overlay_product_layer.hpp>
#include <scwx/qt/gl/draw/linked_vectors.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/view/overlay_product_view.hpp>
#include <scwx/wsr88d/rpg/linked_vector_packet.hpp>
#include <scwx/wsr88d/rpg/rpg_types.hpp>
#include <scwx/wsr88d/rpg/scit_data_packet.hpp>
#include <scwx/wsr88d/rpg/storm_id_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/storm_tracking_information_message.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

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
   explicit Impl(OverlayProductLayer*               self,
                 const std::shared_ptr<MapContext>& context) :
       self_ {self},
       linkedVectors_ {std::make_shared<gl::draw::LinkedVectors>(context)}
   {
   }
   ~Impl() = default;

   void UpdateStormTrackingInformation();

   static void HandleLinkedVectorPacket(
      const std::shared_ptr<const wsr88d::rpg::Packet>& packet,
      const common::Coordinate&                         center,
      const std::string&                                hoverText,
      boost::gil::rgba32f_pixel_t                       color,
      units::length::nautical_miles<float>              tickRadius,
      units::length::nautical_miles<float>              tickRadiusIncrement,
      std::shared_ptr<gl::draw::LinkedVectors>&         linkedVectors);
   static void HandleScitDataPacket(
      const std::shared_ptr<const wsr88d::rpg::StormTrackingInformationMessage>&
                                                        sti,
      const std::shared_ptr<const wsr88d::rpg::Packet>& packet,
      const common::Coordinate&                         center,
      const std::string&                                stormId,
      const std::string&                                hoverText,
      std::shared_ptr<gl::draw::LinkedVectors>&         linkedVectors);

   static void HandleStormIdPacket(
      const std::shared_ptr<const wsr88d::rpg::StormTrackingInformationMessage>&
                                                        sti,
      const std::shared_ptr<const wsr88d::rpg::Packet>& packet,
      std::string&                                      stormId,
      std::string&                                      hoverText);

   static std::string BuildHoverText(
      const std::shared_ptr<
         const scwx::wsr88d::rpg::StormTrackingInformationMessage>& sti,
      std::string&                                                  stormId);

   OverlayProductLayer* self_;

   bool stiNeedsUpdate_ {false};

   std::shared_ptr<gl::draw::LinkedVectors> linkedVectors_;
};

OverlayProductLayer::OverlayProductLayer(std::shared_ptr<MapContext> context) :
    DrawLayer(context), p(std::make_unique<Impl>(this, context))
{
   auto overlayProductView = context->overlay_product_view();
   connect(overlayProductView.get(),
           &view::OverlayProductView::ProductUpdated,
           this,
           [this](std::string product)
           {
              if (product == "NST")
              {
                 p->stiNeedsUpdate_ = true;
              }
           });

   AddDrawItem(p->linkedVectors_);
}

OverlayProductLayer::~OverlayProductLayer() = default;

void OverlayProductLayer::Initialize()
{
   logger_->debug("Initialize()");

   p->UpdateStormTrackingInformation();

   DrawLayer::Initialize();
}

void OverlayProductLayer::Render(
   const QMapLibreGL::CustomLayerRenderParameters& params)
{
   gl::OpenGLFunctions& gl = context()->gl();

   // Set OpenGL blend mode for transparency
   gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   if (p->stiNeedsUpdate_)
   {
      p->UpdateStormTrackingInformation();
   }

   DrawLayer::Render(params);

   SCWX_GL_CHECK_ERROR();
}

void OverlayProductLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");

   DrawLayer::Deinitialize();
}

void OverlayProductLayer::Impl::UpdateStormTrackingInformation()
{
   logger_->debug("Update Storm Tracking Information");

   stiNeedsUpdate_ = false;

   auto overlayProductView  = self_->context()->overlay_product_view();
   auto radarProductManager = overlayProductView->radar_product_manager();
   auto message             = overlayProductView->radar_product_message("NST");

   float latitude  = 0.0f;
   float longitude = 0.0f;

   std::shared_ptr<wsr88d::rpg::StormTrackingInformationMessage> sti = nullptr;
   std::shared_ptr<wsr88d::rpg::ProductSymbologyBlock>           psb = nullptr;
   if (message != nullptr)
   {
      sti = std::dynamic_pointer_cast<
         wsr88d::rpg::StormTrackingInformationMessage>(message);
   }
   if (sti != nullptr)
   {
      psb = sti->symbology_block();
   }

   linkedVectors_->StartVectors();

   if (psb != nullptr)
   {
      std::shared_ptr<config::RadarSite> radarSite = nullptr;
      if (radarProductManager != nullptr)
      {
         radarSite = radarProductManager->radar_site();
      }
      if (radarSite != nullptr)
      {
         latitude  = radarSite->latitude();
         longitude = radarSite->longitude();
      }

      std::string stormId = "?";
      std::string hoverText {};

      for (std::size_t i = 0; i < psb->number_of_layers(); ++i)
      {
         auto packetList = psb->packet_list(static_cast<std::uint16_t>(i));
         for (auto& packet : packetList)
         {
            switch (packet->packet_code())
            {
            case static_cast<std::uint16_t>(wsr88d::rpg::PacketCode::StormId):
               HandleStormIdPacket(sti, packet, stormId, hoverText);
               break;

            case static_cast<std::uint16_t>(
               wsr88d::rpg::PacketCode::ScitPastData):
            case static_cast<std::uint16_t>(
               wsr88d::rpg::PacketCode::ScitForecastData):
               HandleScitDataPacket(sti,
                                    packet,
                                    {latitude, longitude},
                                    stormId,
                                    hoverText,
                                    linkedVectors_);
               break;

            default:
               logger_->trace("Ignoring packet type: {}",
                              packet->packet_code());
               break;
            }
         }
      }
   }
   else
   {
      logger_->trace("No Storm Tracking Information found");
   }

   linkedVectors_->FinishVectors();
}

void OverlayProductLayer::Impl::HandleStormIdPacket(
   const std::shared_ptr<const wsr88d::rpg::StormTrackingInformationMessage>&
                                                     sti,
   const std::shared_ptr<const wsr88d::rpg::Packet>& packet,
   std::string&                                      stormId,
   std::string&                                      hoverText)
{
   auto stormIdPacket =
      std::dynamic_pointer_cast<const wsr88d::rpg::StormIdSymbolPacket>(packet);

   if (stormIdPacket != nullptr && stormIdPacket->RecordCount() > 0)
   {
      stormId   = stormIdPacket->storm_id(0);
      hoverText = BuildHoverText(sti, stormId);
   }
   else
   {
      logger_->warn("Invalid Storm ID Packet");

      stormId = "?";
      hoverText.clear();
   }
}

void OverlayProductLayer::Impl::HandleScitDataPacket(
   const std::shared_ptr<const wsr88d::rpg::StormTrackingInformationMessage>&
                                                     sti,
   const std::shared_ptr<const wsr88d::rpg::Packet>& packet,
   const common::Coordinate&                         center,
   const std::string&                                stormId,
   const std::string&                                hoverText,
   std::shared_ptr<gl::draw::LinkedVectors>&         linkedVectors)
{
   auto scitDataPacket =
      std::dynamic_pointer_cast<const wsr88d::rpg::ScitDataPacket>(packet);

   if (scitDataPacket != nullptr)
   {
      boost::gil::rgba32f_pixel_t color {1.0f, 1.0f, 1.0f, 1.0f};

      units::length::nautical_miles<float> tickRadius {0.5f};
      units::length::nautical_miles<float> tickRadiusIncrement {0.0f};

      auto stiRecord = sti->sti_record(stormId);

      if (scitDataPacket->packet_code() ==
          static_cast<std::uint16_t>(wsr88d::rpg::PacketCode::ScitPastData))
      {
         // If this is past data, the default tick radius and increment with a
         // darker color
         color = {0.5f, 0.5f, 0.5f, 1.0f};
      }
      else if (stiRecord != nullptr && stiRecord->meanError_.has_value())
      {
         // If this is forecast data, use the mean error as the radius (minimum
         // of the default value), incrementing by the mean error
         tickRadiusIncrement = stiRecord->meanError_.value();
         tickRadius          = std::max(tickRadius, tickRadiusIncrement);
      }

      for (auto& subpacket : scitDataPacket->packet_list())
      {
         switch (subpacket->packet_code())
         {
         case static_cast<std::uint16_t>(
            wsr88d::rpg::PacketCode::LinkedVectorNoValue):
            HandleLinkedVectorPacket(subpacket,
                                     center,
                                     hoverText,
                                     color,
                                     tickRadius,
                                     tickRadiusIncrement,
                                     linkedVectors);
            break;

         default:
            logger_->trace("Ignoring SCIT subpacket type: {}",
                           subpacket->packet_code());
            break;
         }
      }
   }
   else
   {
      logger_->warn("Invalid SCIT Data Packet");
   }
}

void OverlayProductLayer::Impl::HandleLinkedVectorPacket(
   const std::shared_ptr<const wsr88d::rpg::Packet>& packet,
   const common::Coordinate&                         center,
   const std::string&                                hoverText,
   boost::gil::rgba32f_pixel_t                       color,
   units::length::nautical_miles<float>              tickRadius,
   units::length::nautical_miles<float>              tickRadiusIncrement,
   std::shared_ptr<gl::draw::LinkedVectors>&         linkedVectors)
{
   auto linkedVectorPacket =
      std::dynamic_pointer_cast<const wsr88d::rpg::LinkedVectorPacket>(packet);

   if (linkedVectorPacket != nullptr)
   {
      auto di = linkedVectors->AddVector(center, linkedVectorPacket);
      gl::draw::LinkedVectors::SetVectorWidth(di, 1.0f);
      gl::draw::LinkedVectors::SetVectorModulate(di, color);
      gl::draw::LinkedVectors::SetVectorHoverText(di, hoverText);
      gl::draw::LinkedVectors::SetVectorTicksEnabled(di, true);
      gl::draw::LinkedVectors::SetVectorTickRadius(di, tickRadius);
      gl::draw::LinkedVectors::SetVectorTickRadiusIncrement(
         di, tickRadiusIncrement);
   }
   else
   {
      logger_->warn("Invalid Linked Vector Packet");
   }
}

std::string OverlayProductLayer::Impl::BuildHoverText(
   const std::shared_ptr<
      const scwx::wsr88d::rpg::StormTrackingInformationMessage>& sti,
   std::string&                                                  stormId)
{
   std::string hoverText = fmt::format("Storm ID: {}", stormId);

   auto stiRecord = sti->sti_record(stormId);

   if (stiRecord != nullptr)
   {
      if (stiRecord->direction_.has_value() && stiRecord->speed_.has_value())
      {
         hoverText +=
            fmt::format("\nMovement: {} @ {}",
                        units::to_string(stiRecord->direction_.value()),
                        units::to_string(stiRecord->speed_.value()));
      }

      if (stiRecord->maxDbz_.has_value() &&
          stiRecord->maxDbzHeight_.has_value())
      {
         hoverText +=
            fmt::format("\nMax dBZ: {} ({} kft)",
                        stiRecord->maxDbz_.value(),
                        stiRecord->maxDbzHeight_.value().value() / 1000.0f);
      }

      if (stiRecord->forecastError_.has_value())
      {
         hoverText +=
            fmt::format("\nForecast Error: {}",
                        units::to_string(stiRecord->forecastError_.value()));
      }

      if (stiRecord->meanError_.has_value())
      {
         hoverText +=
            fmt::format("\nMean Error: {}",
                        units::to_string(stiRecord->meanError_.value()));
      }
   }

   auto dateTime = sti->date_time();
   if (dateTime.has_value())
   {
      hoverText +=
         fmt::format("\nDate/Time: {}", util::TimeString(dateTime.value()));
   }

   auto forecastInterval = sti->forecast_interval();
   if (forecastInterval.has_value())
   {
      hoverText += fmt::format("\nForecast Interval: {} min",
                               forecastInterval.value().count());
   }

   return hoverText;
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
