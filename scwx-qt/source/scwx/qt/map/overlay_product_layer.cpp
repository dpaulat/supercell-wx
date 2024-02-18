#include <scwx/qt/map/overlay_product_layer.hpp>
#include <scwx/qt/gl/draw/linked_vectors.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/view/overlay_product_view.hpp>
#include <scwx/wsr88d/rpg/graphic_product_message.hpp>
#include <scwx/wsr88d/rpg/linked_vector_packet.hpp>
#include <scwx/wsr88d/rpg/rpg_types.hpp>
#include <scwx/wsr88d/rpg/scit_data_packet.hpp>
#include <scwx/wsr88d/rpg/storm_id_symbol_packet.hpp>
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
   explicit Impl(OverlayProductLayer*               self,
                 const std::shared_ptr<MapContext>& context) :
       self_ {self},
       linkedVectors_ {std::make_shared<gl::draw::LinkedVectors>(context)}
   {
   }
   ~Impl() = default;

   void UpdateStormTrackingInformation();

   static void HandleLinkedVectorPacket(
      const std::shared_ptr<wsr88d::rpg::Packet>& packet,
      const common::Coordinate&                   center,
      const std::string&                          hoverText,
      boost::gil::rgba32f_pixel_t                 color,
      bool                                        tickRadiusIncrement,
      std::shared_ptr<gl::draw::LinkedVectors>&   linkedVectors);
   static void HandleScitDataPacket(
      const std::shared_ptr<wsr88d::rpg::Packet>& packet,
      const common::Coordinate&                   center,
      const std::string&                          stormId,
      std::shared_ptr<gl::draw::LinkedVectors>&   linkedVectors);
   static void
   HandleStormIdPacket(const std::shared_ptr<wsr88d::rpg::Packet>& packet,
                       std::string&                                stormId);

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
   auto record              = overlayProductView->radar_product_record("NST");

   float latitude  = 0.0f;
   float longitude = 0.0f;

   std::shared_ptr<wsr88d::Level3File>                 l3File = nullptr;
   std::shared_ptr<wsr88d::rpg::GraphicProductMessage> gpm    = nullptr;
   std::shared_ptr<wsr88d::rpg::ProductSymbologyBlock> psb    = nullptr;
   if (record != nullptr)
   {
      l3File = record->level3_file();
   }
   if (l3File != nullptr)
   {
      gpm = std::dynamic_pointer_cast<wsr88d::rpg::GraphicProductMessage>(
         l3File->message());
   }
   if (gpm != nullptr)
   {
      psb = gpm->symbology_block();
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

      for (std::size_t i = 0; i < psb->number_of_layers(); ++i)
      {
         auto packetList = psb->packet_list(static_cast<std::uint16_t>(i));
         for (auto& packet : packetList)
         {
            switch (packet->packet_code())
            {
            case static_cast<std::uint16_t>(wsr88d::rpg::PacketCode::StormId):
               HandleStormIdPacket(packet, stormId);
               break;

            case static_cast<std::uint16_t>(
               wsr88d::rpg::PacketCode::ScitPastData):
            case static_cast<std::uint16_t>(
               wsr88d::rpg::PacketCode::ScitForecastData):
               HandleScitDataPacket(
                  packet, {latitude, longitude}, stormId, linkedVectors_);
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
   const std::shared_ptr<wsr88d::rpg::Packet>& packet, std::string& stormId)
{
   auto stormIdPacket =
      std::dynamic_pointer_cast<wsr88d::rpg::StormIdSymbolPacket>(packet);

   if (stormIdPacket != nullptr && stormIdPacket->RecordCount() > 0)
   {
      stormId = stormIdPacket->storm_id(0);
   }
   else
   {
      logger_->warn("Invalid Storm ID Packet");

      stormId = "?";
   }
}

void OverlayProductLayer::Impl::HandleScitDataPacket(
   const std::shared_ptr<wsr88d::rpg::Packet>& packet,
   const common::Coordinate&                   center,
   const std::string&                          stormId,
   std::shared_ptr<gl::draw::LinkedVectors>&   linkedVectors)
{
   auto scitDataPacket =
      std::dynamic_pointer_cast<wsr88d::rpg::ScitDataPacket>(packet);

   if (scitDataPacket != nullptr)
   {
      boost::gil::rgba32f_pixel_t color {1.0f, 1.0f, 1.0f, 1.0f};
      bool                        tickRadiusIncrement = true;
      if (scitDataPacket->packet_code() ==
          static_cast<std::uint16_t>(wsr88d::rpg::PacketCode::ScitPastData))
      {
         color               = {0.5f, 0.5f, 0.5f, 1.0f};
         tickRadiusIncrement = false;
      }

      for (auto& subpacket : scitDataPacket->packet_list())
      {
         switch (subpacket->packet_code())
         {
         case static_cast<std::uint16_t>(
            wsr88d::rpg::PacketCode::LinkedVectorNoValue):
            HandleLinkedVectorPacket(subpacket,
                                     center,
                                     stormId,
                                     color,
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
   const std::shared_ptr<wsr88d::rpg::Packet>& packet,
   const common::Coordinate&                   center,
   const std::string&                          hoverText,
   boost::gil::rgba32f_pixel_t                 color,
   bool                                        tickRadiusIncrement,
   std::shared_ptr<gl::draw::LinkedVectors>&   linkedVectors)
{
   auto linkedVectorPacket =
      std::dynamic_pointer_cast<wsr88d::rpg::LinkedVectorPacket>(packet);

   if (linkedVectorPacket != nullptr)
   {
      auto di = linkedVectors->AddVector(center, linkedVectorPacket);
      gl::draw::LinkedVectors::SetVectorWidth(di, 1.0f);
      gl::draw::LinkedVectors::SetVectorModulate(di, color);
      gl::draw::LinkedVectors::SetVectorHoverText(di, hoverText);
      gl::draw::LinkedVectors::SetVectorTicksEnabled(di, true);
      gl::draw::LinkedVectors::SetVectorTickRadius(
         di, units::length::nautical_miles<double> {1.0});

      if (tickRadiusIncrement)
      {
         gl::draw::LinkedVectors::SetVectorTickRadiusIncrement(
            di, units::length::nautical_miles<double> {1.0});
      }
      else
      {
         gl::draw::LinkedVectors::SetVectorTickRadiusIncrement(
            di, units::length::nautical_miles<double> {0.0});
      }
   }
   else
   {
      logger_->warn("Invalid Linked Vector Packet");
   }
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
