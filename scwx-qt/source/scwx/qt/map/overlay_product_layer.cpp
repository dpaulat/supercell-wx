#include <scwx/qt/map/overlay_product_layer.hpp>
#include <scwx/qt/gl/draw/geo_icons.hpp>
#include <scwx/qt/gl/draw/linked_vectors.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/qt/map/overlay_product_symbols.hpp>
#include <scwx/qt/settings/product_settings.hpp>
#include <scwx/qt/types/texture_types.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/qt/view/overlay_product_view.hpp>
#include <scwx/wsr88d/rpg/graphic_product_message.hpp>
#include <scwx/wsr88d/rpg/hda_hail_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/linked_vector_packet.hpp>
#include <scwx/wsr88d/rpg/mesocyclone_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/point_feature_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/point_graphic_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/rpg_types.hpp>
#include <scwx/wsr88d/rpg/scit_data_packet.hpp>
#include <scwx/wsr88d/rpg/storm_id_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/storm_tracking_information_message.hpp>
#include <scwx/wsr88d/rpg/text_and_special_symbol_packet.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <algorithm>
#include <limits>

#include <fmt/format.h>

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::overlay_product_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

// RPG I/J coordinates are 1/4 km (ICD)
static constexpr double kKmPerIPosition = 0.25;

class OverlayProductLayer::Impl
{
public:
   explicit Impl(OverlayProductLayer*                  self,
                 const std::shared_ptr<gl::GlContext>& glContext) :
       self_ {self},
       linkedVectors_ {std::make_shared<gl::draw::LinkedVectors>(glContext)},
       geoIcons_ {std::make_shared<gl::draw::GeoIcons>(glContext)}
   {
      auto& productSettings = settings::ProductSettings::Instance();

      stiForecastEnabledCallbackUuid_ =
         productSettings.sti_forecast_enabled().RegisterValueStagedCallback(
            [=, this](const bool& value)
            {
               stiForecastEnabled_ = value;
               overlaysNeedUpdate_ = true;
               Q_EMIT self_->NeedsRendering();
            });
      stiPastEnabledCallbackUuid_ =
         productSettings.sti_past_enabled().RegisterValueStagedCallback(
            [=, this](const bool& value)
            {
               stiPastEnabled_     = value;
               overlaysNeedUpdate_ = true;
               Q_EMIT self_->NeedsRendering();
            });
      hailIndexEnabledCallbackUuid_ =
         productSettings.hail_index_enabled().RegisterValueStagedCallback(
            [=, this](const bool& value)
            {
               hailIndexEnabled_   = value;
               overlaysNeedUpdate_ = true;
               Q_EMIT self_->NeedsRendering();
            });
      mesocycloneEnabledCallbackUuid_ =
         productSettings.mesocyclone_enabled().RegisterValueStagedCallback(
            [=, this](const bool& value)
            {
               mesocycloneEnabled_ = value;
               overlaysNeedUpdate_ = true;
               Q_EMIT self_->NeedsRendering();
            });
      tvsEnabledCallbackUuid_ =
         productSettings.tvs_enabled().RegisterValueStagedCallback(
            [=, this](const bool& value)
            {
               tvsEnabled_         = value;
               overlaysNeedUpdate_ = true;
               Q_EMIT self_->NeedsRendering();
            });

      stiForecastEnabled_ =
         productSettings.sti_forecast_enabled().GetStagedOrValue();
      stiPastEnabled_ = productSettings.sti_past_enabled().GetStagedOrValue();
      hailIndexEnabled_ =
         productSettings.hail_index_enabled().GetStagedOrValue();
      mesocycloneEnabled_ =
         productSettings.mesocyclone_enabled().GetStagedOrValue();
      tvsEnabled_ = productSettings.tvs_enabled().GetStagedOrValue();
   }
   ~Impl()
   {
      auto& productSettings = settings::ProductSettings::Instance();

      productSettings.sti_forecast_enabled().UnregisterValueStagedCallback(
         stiForecastEnabledCallbackUuid_);
      productSettings.sti_past_enabled().UnregisterValueStagedCallback(
         stiPastEnabledCallbackUuid_);
      productSettings.hail_index_enabled().UnregisterValueStagedCallback(
         hailIndexEnabledCallbackUuid_);
      productSettings.mesocyclone_enabled().UnregisterValueStagedCallback(
         mesocycloneEnabledCallbackUuid_);
      productSettings.tvs_enabled().UnregisterValueStagedCallback(
         tvsEnabledCallbackUuid_);
   }

   void SetupIconSheets();
   void UpdateOverlays(const std::shared_ptr<MapContext>& mapContext);
   void UpdateStormTrackingInformation(
      const std::shared_ptr<MapContext>& mapContext);
   void UpdateGraphicOverlay(const std::shared_ptr<MapContext>& mapContext,
                             const std::string&                 product);

   static common::Coordinate SymbolCoordinate(const common::Coordinate& center,
                                              std::int16_t iPosition,
                                              std::int16_t jPosition);

   static std::string FindAssociatedLabel(
      const std::vector<std::shared_ptr<wsr88d::rpg::Packet>>& packets,
      std::int16_t                                             iPosition,
      std::int16_t                                             jPosition);

   static void HandleLinkedVectorPacket(
      const std::shared_ptr<const wsr88d::rpg::Packet>& packet,
      const common::Coordinate&                         center,
      const std::string&                                hoverText,
      boost::gil::rgba32f_pixel_t                       color,
      units::length::nautical_miles<float>              tickRadius,
      units::length::nautical_miles<float>              tickRadiusIncrement,
      std::shared_ptr<gl::draw::LinkedVectors>&         linkedVectors);
   void HandleScitDataPacket(
      const std::shared_ptr<const wsr88d::rpg::StormTrackingInformationMessage>&
                                                        sti,
      const std::shared_ptr<const wsr88d::rpg::Packet>& packet,
      const common::Coordinate&                         center,
      const std::string&                                stormId,
      const std::string&                                hoverText,
      std::shared_ptr<gl::draw::LinkedVectors>&         linkedVectors);
   void HandleOverlayScitDataPacket(
      const std::shared_ptr<const wsr88d::rpg::Packet>& packet,
      const common::Coordinate&                         center,
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

   boost::uuids::uuid stiForecastEnabledCallbackUuid_;
   boost::uuids::uuid stiPastEnabledCallbackUuid_;
   boost::uuids::uuid hailIndexEnabledCallbackUuid_;
   boost::uuids::uuid mesocycloneEnabledCallbackUuid_;
   boost::uuids::uuid tvsEnabledCallbackUuid_;

   bool stiForecastEnabled_ {true};
   bool stiPastEnabled_ {true};
   bool hailIndexEnabled_ {true};
   bool mesocycloneEnabled_ {true};
   bool tvsEnabled_ {true};

   bool overlaysNeedUpdate_ {false};

   std::shared_ptr<gl::draw::LinkedVectors> linkedVectors_;
   std::shared_ptr<gl::draw::GeoIcons>      geoIcons_;

   std::string hailIconSheet_ {
      types::GetTextureName(types::ImageTexture::HailIndex)};
   std::string mesoIconSheet_ {
      types::GetTextureName(types::ImageTexture::Mesocyclone)};
   std::string tvsIconSheet_ {
      types::GetTextureName(types::ImageTexture::TornadicVortexSignature)};
};

OverlayProductLayer::OverlayProductLayer(
   const std::shared_ptr<gl::GlContext>& glContext) :
    DrawLayer(glContext, "OverlayProductLayer"),
    p(std::make_unique<Impl>(this, glContext))
{
   AddDrawItem(p->linkedVectors_);
   AddDrawItem(p->geoIcons_);
}

OverlayProductLayer::~OverlayProductLayer() = default;

void OverlayProductLayer::Initialize(
   const std::shared_ptr<MapContext>& mapContext)
{
   logger_->debug("Initialize()");

   auto overlayProductView = mapContext->overlay_product_view();
   connect(overlayProductView.get(),
           &view::OverlayProductView::ProductUpdated,
           this,
           [this](std::string product)
           {
              if (IsOverlayProduct(product))
              {
                 p->overlaysNeedUpdate_ = true;
                 Q_EMIT NeedsRendering();
              }
           });

   p->SetupIconSheets();
   p->UpdateOverlays(mapContext);

   DrawLayer::Initialize(mapContext);
}

void OverlayProductLayer::Render(
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   if (p->overlaysNeedUpdate_)
   {
      p->UpdateOverlays(mapContext);
   }

   DrawLayer::Render(mapContext, params);

   SCWX_GL_CHECK_ERROR();
}

void OverlayProductLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");

   disconnect(this);

   DrawLayer::Deinitialize();
}

void OverlayProductLayer::Impl::SetupIconSheets()
{
   geoIcons_->StartIconSheets();
   geoIcons_->AddIconSheet(
      hailIconSheet_, kOverlayIconWidth, kOverlayIconHeight);
   geoIcons_->AddIconSheet(
      mesoIconSheet_, kOverlayIconWidth, kOverlayIconHeight);
   geoIcons_->AddIconSheet(
      tvsIconSheet_, kOverlayIconWidth, kOverlayIconHeight);
   geoIcons_->FinishIconSheets();
}

void OverlayProductLayer::Impl::UpdateOverlays(
   const std::shared_ptr<MapContext>& mapContext)
{
   overlaysNeedUpdate_ = false;

   linkedVectors_->StartVectors();
   geoIcons_->StartIcons();

   UpdateStormTrackingInformation(mapContext);

   if (hailIndexEnabled_)
   {
      UpdateGraphicOverlay(mapContext, std::string {kNhiProduct});
   }
   if (mesocycloneEnabled_)
   {
      UpdateGraphicOverlay(mapContext, std::string {kNmdProduct});
      UpdateGraphicOverlay(mapContext, std::string {kNmeProduct});
   }
   if (tvsEnabled_)
   {
      UpdateGraphicOverlay(mapContext, std::string {kNtvProduct});
   }

   linkedVectors_->FinishVectors();
   geoIcons_->FinishIcons();
}

common::Coordinate
OverlayProductLayer::Impl::SymbolCoordinate(const common::Coordinate& center,
                                            std::int16_t              iPosition,
                                            std::int16_t              jPosition)
{
   return util::GeographicLib::GetCoordinate(
      center,
      units::kilometers<double> {iPosition * kKmPerIPosition},
      units::kilometers<double> {jPosition * kKmPerIPosition});
}

std::string OverlayProductLayer::Impl::FindAssociatedLabel(
   const std::vector<std::shared_ptr<wsr88d::rpg::Packet>>& packets,
   std::int16_t                                             iPosition,
   std::int16_t                                             jPosition)
{
   std::string  nearestLabel;
   std::int64_t nearestDistance = std::numeric_limits<std::int64_t>::max();

   auto consider =
      [&](std::int16_t labelI, std::int16_t labelJ, const std::string& label)
   {
      if (label.empty())
      {
         return;
      }

      const std::int64_t di = static_cast<std::int64_t>(labelI) - iPosition;
      const std::int64_t dj = static_cast<std::int64_t>(labelJ) - jPosition;
      const std::int64_t distanceSquared = di * di + dj * dj;

      if (distanceSquared < nearestDistance)
      {
         nearestDistance = distanceSquared;
         nearestLabel    = label;
      }
   };

   for (const auto& packet : packets)
   {
      const auto packetCode = packet->packet_code();

      if (packetCode ==
          static_cast<std::uint16_t>(wsr88d::rpg::PacketCode::StormId))
      {
         auto stormIdPacket =
            std::dynamic_pointer_cast<const wsr88d::rpg::StormIdSymbolPacket>(
               packet);
         if (stormIdPacket == nullptr)
         {
            continue;
         }

         for (std::size_t r = 0; r < stormIdPacket->RecordCount(); ++r)
         {
            consider(stormIdPacket->i_position(r),
                     stormIdPacket->j_position(r),
                     stormIdPacket->storm_id(r));
         }
      }
      else if (packetCode == static_cast<std::uint16_t>(
                                wsr88d::rpg::PacketCode::TextUniform) ||
               packetCode == static_cast<std::uint16_t>(
                                wsr88d::rpg::PacketCode::TextNoValue))
      {
         auto textPacket = std::dynamic_pointer_cast<
            const wsr88d::rpg::TextAndSpecialSymbolPacket>(packet);
         if (textPacket == nullptr ||
             textPacket->special_symbol() != wsr88d::rpg::SpecialSymbol::None)
         {
            continue;
         }

         consider(textPacket->start_i(),
                  textPacket->start_j(),
                  TrimLabel(textPacket->text()));
      }
   }

   // Exact I/J matches first; otherwise allow nearby labels (TVS vs storm ID)
   constexpr std::int64_t kMaxDistanceSquared = 40 * 40;
   if (nearestDistance <= kMaxDistanceSquared)
   {
      return nearestLabel;
   }

   return {};
}

void OverlayProductLayer::Impl::UpdateGraphicOverlay(
   const std::shared_ptr<MapContext>& mapContext, const std::string& product)
{
   auto overlayProductView  = mapContext->overlay_product_view();
   auto radarProductManager = overlayProductView->radar_product_manager();
   auto message = overlayProductView->radar_product_message(product);

   std::shared_ptr<wsr88d::rpg::GraphicProductMessage> gpm = nullptr;
   std::shared_ptr<wsr88d::rpg::ProductSymbologyBlock> psb = nullptr;
   if (message != nullptr)
   {
      gpm =
         std::dynamic_pointer_cast<wsr88d::rpg::GraphicProductMessage>(message);
   }
   if (gpm != nullptr)
   {
      psb = gpm->symbology_block();
   }
   if (psb == nullptr)
   {
      return;
   }

   float latitude  = 0.0f;
   float longitude = 0.0f;
   if (radarProductManager != nullptr)
   {
      auto radarSite = radarProductManager->radar_site();
      if (radarSite != nullptr)
      {
         latitude  = radarSite->latitude();
         longitude = radarSite->longitude();
      }
   }
   const common::Coordinate center {latitude, longitude};

   for (std::size_t layer = 0; layer < psb->number_of_layers(); ++layer)
   {
      auto packetList = psb->packet_list(static_cast<std::uint16_t>(layer));
      std::string lastLabel;

      for (auto& packet : packetList)
      {
         const auto packetCode = packet->packet_code();

         switch (packetCode)
         {
         case static_cast<std::uint16_t>(
            wsr88d::rpg::PacketCode::HdaHailSymbol):
         {
            auto hailPacket = std::dynamic_pointer_cast<
               const wsr88d::rpg::HdaHailSymbolPacket>(packet);
            if (hailPacket == nullptr)
            {
               break;
            }

            for (std::size_t r = 0; r < hailPacket->RecordCount(); ++r)
            {
               if (!HailSymbolVisible(hailPacket->probability_of_hail(r)))
               {
                  continue;
               }

               const std::string label =
                  FindAssociatedLabel(packetList,
                                      hailPacket->i_position(r),
                                      hailPacket->j_position(r));
               if (!label.empty())
               {
                  lastLabel = label;
               }

               auto di = geoIcons_->AddIcon();
               geoIcons_->SetIconTexture(
                  di,
                  hailIconSheet_,
                  HailIconIndex(hailPacket->probability_of_hail(r),
                                hailPacket->probability_of_severe_hail(r),
                                hailPacket->max_hail_size(r)));
               const auto coord = SymbolCoordinate(
                  center, hailPacket->i_position(r), hailPacket->j_position(r));
               geoIcons_->SetIconLocation(
                  di, coord.latitude_, coord.longitude_);
               geoIcons_->SetIconHoverText(
                  di,
                  HailHoverText(label,
                                hailPacket->probability_of_hail(r),
                                hailPacket->probability_of_severe_hail(r),
                                hailPacket->max_hail_size(r)));
            }
            break;
         }

         case static_cast<std::uint16_t>(
            wsr88d::rpg::PacketCode::HailPositiveSymbol):
         case static_cast<std::uint16_t>(
            wsr88d::rpg::PacketCode::HailProbableSymbol):
         {
            auto pointPacket = std::dynamic_pointer_cast<
               const wsr88d::rpg::PointGraphicSymbolPacket>(packet);
            if (pointPacket == nullptr)
            {
               break;
            }

            const bool positive =
               packetCode == static_cast<std::uint16_t>(
                                wsr88d::rpg::PacketCode::HailPositiveSymbol);
            for (std::size_t r = 0; r < pointPacket->RecordCount(); ++r)
            {
               auto di = geoIcons_->AddIcon();
               geoIcons_->SetIconTexture(di,
                                         hailIconSheet_,
                                         positive ? kHailIconSevere :
                                                    kHailIconSmall);
               const auto coord = SymbolCoordinate(center,
                                                   pointPacket->i_position(r),
                                                   pointPacket->j_position(r));
               geoIcons_->SetIconLocation(
                  di, coord.latitude_, coord.longitude_);
               geoIcons_->SetIconHoverText(
                  di, positive ? "Hail (Positive)" : "Hail (Probable)");
            }
            break;
         }

         case static_cast<std::uint16_t>(
            wsr88d::rpg::PacketCode::PointFeatureSymbol):
         {
            auto featurePacket = std::dynamic_pointer_cast<
               const wsr88d::rpg::PointFeatureSymbolPacket>(packet);
            if (featurePacket == nullptr)
            {
               break;
            }

            for (std::size_t r = 0; r < featurePacket->RecordCount(); ++r)
            {
               const auto featureType = featurePacket->point_feature_type(r);
               if (!IsTvsFeatureType(featureType) &&
                   !IsMesocycloneFeatureType(featureType))
               {
                  continue;
               }

               const std::string label =
                  FindAssociatedLabel(packetList,
                                      featurePacket->i_position(r),
                                      featurePacket->j_position(r));
               if (!label.empty())
               {
                  lastLabel = label;
               }

               const auto coord =
                  SymbolCoordinate(center,
                                   featurePacket->i_position(r),
                                   featurePacket->j_position(r));

               auto di = geoIcons_->AddIcon();
               geoIcons_->SetIconLocation(
                  di, coord.latitude_, coord.longitude_);

               if (IsTvsFeatureType(featureType))
               {
                  geoIcons_->SetIconTexture(
                     di,
                     tvsIconSheet_,
                     TvsIconIndexFromFeatureType(featureType));
                  geoIcons_->SetIconHoverText(
                     di,
                     TvsHoverText(label, PointFeatureTypeName(featureType)));
               }
               else
               {
                  geoIcons_->SetIconTexture(
                     di,
                     mesoIconSheet_,
                     MesocycloneIconIndexFromFeatureType(featureType));
                  geoIcons_->SetIconHoverText(
                     di,
                     MesocycloneHoverText(
                        label,
                        featureType,
                        featurePacket->point_feature_attribute(r)));
               }
            }
            break;
         }

         case static_cast<std::uint16_t>(
            wsr88d::rpg::PacketCode::MesocycloneSymbol3):
         case static_cast<std::uint16_t>(
            wsr88d::rpg::PacketCode::MesocycloneSymbol11):
         {
            auto mesoPacket = std::dynamic_pointer_cast<
               const wsr88d::rpg::MesocycloneSymbolPacket>(packet);
            if (mesoPacket == nullptr)
            {
               break;
            }

            for (std::size_t r = 0; r < mesoPacket->RecordCount(); ++r)
            {
               if (mesoPacket->radius_of_mesocyclone(r) == 0 &&
                   mesoPacket->i_position(r) == 0 &&
                   mesoPacket->j_position(r) == 0)
               {
                  continue;
               }

               const std::string label =
                  FindAssociatedLabel(packetList,
                                      mesoPacket->i_position(r),
                                      mesoPacket->j_position(r));
               if (!label.empty())
               {
                  lastLabel = label;
               }

               auto di = geoIcons_->AddIcon();
               geoIcons_->SetIconTexture(
                  di,
                  mesoIconSheet_,
                  MesocycloneIconIndexFromPacketCode(packetCode));
               const auto coord = SymbolCoordinate(
                  center, mesoPacket->i_position(r), mesoPacket->j_position(r));
               geoIcons_->SetIconLocation(
                  di, coord.latitude_, coord.longitude_);
               geoIcons_->SetIconHoverText(
                  di,
                  LegacyMesocycloneHoverText(
                     label, packetCode, mesoPacket->radius_of_mesocyclone(r)));
            }
            break;
         }

         case static_cast<std::uint16_t>(
            wsr88d::rpg::PacketCode::TornadoVortexSignatureSymbol):
         case static_cast<std::uint16_t>(
            wsr88d::rpg::PacketCode::ElevatedTornadoVortexSignatureSymbol):
         {
            auto tvsPacket = std::dynamic_pointer_cast<
               const wsr88d::rpg::PointGraphicSymbolPacket>(packet);
            if (tvsPacket == nullptr)
            {
               break;
            }

            for (std::size_t r = 0; r < tvsPacket->RecordCount(); ++r)
            {
               const std::string label =
                  FindAssociatedLabel(packetList,
                                      tvsPacket->i_position(r),
                                      tvsPacket->j_position(r));
               if (!label.empty())
               {
                  lastLabel = label;
               }

               auto di = geoIcons_->AddIcon();
               geoIcons_->SetIconTexture(
                  di, tvsIconSheet_, TvsIconIndexFromPacketCode(packetCode));
               const auto coord = SymbolCoordinate(
                  center, tvsPacket->i_position(r), tvsPacket->j_position(r));
               geoIcons_->SetIconLocation(
                  di, coord.latitude_, coord.longitude_);
               geoIcons_->SetIconHoverText(
                  di, TvsHoverText(label, TvsTypeName(packetCode)));
            }
            break;
         }

         case static_cast<std::uint16_t>(wsr88d::rpg::PacketCode::ScitPastData):
         case static_cast<std::uint16_t>(
            wsr88d::rpg::PacketCode::ScitForecastData):
         {
            // MDA includes past/forecast tracks; reuse STI vector drawing
            if (packetCode == static_cast<std::uint16_t>(
                                 wsr88d::rpg::PacketCode::ScitPastData) &&
                !stiPastEnabled_)
            {
               break;
            }
            if (packetCode == static_cast<std::uint16_t>(
                                 wsr88d::rpg::PacketCode::ScitForecastData) &&
                !stiForecastEnabled_)
            {
               break;
            }

            HandleOverlayScitDataPacket(
               packet, center, lastLabel, linkedVectors_);
            break;
         }

         default:
            break;
         }
      }
   }

   logger_->trace("Updated overlay product {}", product);
}

void OverlayProductLayer::Impl::HandleOverlayScitDataPacket(
   const std::shared_ptr<const wsr88d::rpg::Packet>& packet,
   const common::Coordinate&                         center,
   const std::string&                                hoverText,
   std::shared_ptr<gl::draw::LinkedVectors>&         linkedVectors)
{
   auto scitDataPacket =
      std::dynamic_pointer_cast<const wsr88d::rpg::ScitDataPacket>(packet);
   if (scitDataPacket == nullptr)
   {
      return;
   }

   boost::gil::rgba32f_pixel_t color {1.0f, 0.65f, 0.0f, 1.0f};
   if (scitDataPacket->packet_code() ==
       static_cast<std::uint16_t>(wsr88d::rpg::PacketCode::ScitPastData))
   {
      color = {0.7f, 0.45f, 0.0f, 1.0f};
   }

   units::length::nautical_miles<float> tickRadius {0.5f};
   units::length::nautical_miles<float> tickRadiusIncrement {0.0f};

   for (auto& subpacket : scitDataPacket->packet_list())
   {
      if (subpacket->packet_code() ==
          static_cast<std::uint16_t>(
             wsr88d::rpg::PacketCode::LinkedVectorNoValue))
      {
         HandleLinkedVectorPacket(subpacket,
                                  center,
                                  hoverText,
                                  color,
                                  tickRadius,
                                  tickRadiusIncrement,
                                  linkedVectors);
      }
   }
}

void OverlayProductLayer::Impl::UpdateStormTrackingInformation(
   const std::shared_ptr<MapContext>& mapContext)
{
   logger_->debug("Update Storm Tracking Information");

   auto overlayProductView  = mapContext->overlay_product_view();
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
         if (!stiPastEnabled_)
         {
            return;
         }

         // If this is past data, the default tick radius and increment with a
         // darker color
         color = {0.5f, 0.5f, 0.5f, 1.0f};
      }
      else
      {
         if (!stiForecastEnabled_)
         {
            return;
         }

         if (stiRecord != nullptr && stiRecord->meanError_.has_value())
         {
            // If this is forecast data, use the mean error as the radius
            // (minimum of the default value), incrementing by the mean error
            tickRadiusIncrement = stiRecord->meanError_.value();
            tickRadius          = std::max(tickRadius, tickRadiusIncrement);
         }
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
      hoverText += fmt::format("\nDate/Time: {}",
                               scwx::util::TimeString(dateTime.value()));
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
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params,
   const QPointF&                                mouseLocalPos,
   const QPointF&                                mouseGlobalPos,
   const glm::vec2&                              mouseCoords,
   const common::Coordinate&                     mouseGeoCoords,
   std::shared_ptr<types::EventHandler>&         eventHandler)
{
   return DrawLayer::RunMousePicking(mapContext,
                                     params,
                                     mouseLocalPos,
                                     mouseGlobalPos,
                                     mouseCoords,
                                     mouseGeoCoords,
                                     eventHandler);
}

} // namespace scwx::qt::map
