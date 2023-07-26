#include <scwx/qt/map/placefile_layer.hpp>
#include <scwx/qt/manager/placefile_manager.hpp>
#include <scwx/qt/manager/resource_manager.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/util/logger.hpp>

#include <fmt/format.h>
#include <imgui.h>
#include <mbgl/util/constants.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ = "scwx::qt::map::placefile_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class PlacefileLayer::Impl
{
public:
   explicit Impl() {};
   ~Impl() = default;

   void
   RenderTextDrawItem(const QMapLibreGL::CustomLayerRenderParameters& params,
                      std::shared_ptr<gr::Placefile::TextDrawItem>    di);

   void RenderText(const QMapLibreGL::CustomLayerRenderParameters& params,
                   const std::string&                              text,
                   const std::string&                              hoverText,
                   boost::gil::rgba8_pixel_t                       color,
                   float                                           x,
                   float                                           y);

   std::uint32_t textId_ {};
   glm::vec2     mapScreenCoordLocation_ {};
   float         mapScale_ {1.0f};
   float         halfWidth_ {};
   float         halfHeight_ {};
   bool          thresholded_ {true};
   ImFont*       monospaceFont_ {};
};

PlacefileLayer::PlacefileLayer(std::shared_ptr<MapContext> context) :
    DrawLayer(context), p(std::make_unique<PlacefileLayer::Impl>())
{
}

PlacefileLayer::~PlacefileLayer() = default;

void PlacefileLayer::Initialize()
{
   logger_->debug("Initialize()");

   DrawLayer::Initialize();
}

void PlacefileLayer::Impl::RenderTextDrawItem(
   const QMapLibreGL::CustomLayerRenderParameters& params,
   std::shared_ptr<gr::Placefile::TextDrawItem>    di)
{
   auto distance =
      (thresholded_) ?
         util::GeographicLib::GetDistance(
            params.latitude, params.longitude, di->latitude_, di->longitude_) :
         0;

   if (distance < di->threshold_)
   {
      const auto screenCoordinates = (util::maplibre::LatLongToScreenCoordinate(
                                         {di->latitude_, di->longitude_}) -
                                      mapScreenCoordLocation_) *
                                     mapScale_;

      RenderText(params,
                 di->text_,
                 di->hoverText_,
                 di->color_,
                 screenCoordinates.x + di->x_ + halfWidth_,
                 screenCoordinates.y + di->y_ + halfHeight_);
   }
}

void PlacefileLayer::Impl::RenderText(
   const QMapLibreGL::CustomLayerRenderParameters& params,
   const std::string&                              text,
   const std::string&                              hoverText,
   boost::gil::rgba8_pixel_t                       color,
   float                                           x,
   float                                           y)
{
   const std::string windowName {fmt::format("PlacefileText-{}", ++textId_)};

   // Convert screen to ImGui coordinates
   y = params.height - y;

   // Setup "window" to hold text
   ImGui::SetNextWindowPos(
      ImVec2 {x, y}, ImGuiCond_Always, ImVec2 {0.5f, 0.5f});
   ImGui::Begin(windowName.c_str(),
                nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
                   ImGuiWindowFlags_NoBackground);

   // Render text
   ImGui::PushStyleColor(ImGuiCol_Text,
                         IM_COL32(color[0], color[1], color[2], color[3]));
   ImGui::TextUnformatted(text.c_str());
   ImGui::PopStyleColor();

   // Create tooltip for hover text
   if (!hoverText.empty() && ImGui::IsItemHovered())
   {
      ImGui::BeginTooltip();
      ImGui::PushFont(monospaceFont_);
      ImGui::TextUnformatted(hoverText.c_str());
      ImGui::PopFont();
      ImGui::EndTooltip();
   }

   // End window
   ImGui::End();
}

void PlacefileLayer::Render(
   const QMapLibreGL::CustomLayerRenderParameters& params)
{
   gl::OpenGLFunctions& gl = context()->gl();

   DrawLayer::Render(params);

   // Reset text ID per frame
   p->textId_ = 0;

   // Update map screen coordinate and scale information
   p->mapScreenCoordLocation_ = util::maplibre::LatLongToScreenCoordinate(
      {params.latitude, params.longitude});
   p->mapScale_ = std::pow(2.0, params.zoom) * mbgl::util::tileSize_D /
                  mbgl::util::DEGREES_MAX;
   p->halfWidth_  = params.width * 0.5f;
   p->halfHeight_ = params.height * 0.5f;

   // Get monospace font pointer
   std::size_t fontSize = 16;
   auto        fontSizes =
      manager::SettingsManager::general_settings().font_sizes().GetValue();
   if (fontSizes.size() > 1)
   {
      fontSize = fontSizes[1];
   }
   else if (fontSizes.size() > 0)
   {
      fontSize = fontSizes[0];
   }
   auto monospace =
      manager::ResourceManager::Font(types::Font::Inconsolata_Regular);
   p->monospaceFont_ = monospace->ImGuiFont(fontSize);

   std::shared_ptr<manager::PlacefileManager> placefileManager =
      manager::PlacefileManager::Instance();

   // Render text
   for (auto& placefile : placefileManager->GetActivePlacefiles())
   {
      p->thresholded_ =
         placefileManager->placefile_thresholded(placefile->name());

      for (auto& drawItem : placefile->GetDrawItems())
      {
         switch (drawItem->itemType_)
         {
         case gr::Placefile::ItemType::Text:
            p->RenderTextDrawItem(
               params,
               std::static_pointer_cast<gr::Placefile::TextDrawItem>(drawItem));
            break;

         default:
            break;
         }
      }
   }

   SCWX_GL_CHECK_ERROR();
}

void PlacefileLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");

   DrawLayer::Deinitialize();
}

} // namespace map
} // namespace qt
} // namespace scwx
