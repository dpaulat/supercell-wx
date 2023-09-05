#include <scwx/qt/gl/draw/placefile_text.hpp>
#include <scwx/qt/util/imgui.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/util/logger.hpp>

#include <fmt/format.h>
#include <imgui.h>
#include <mbgl/util/constants.hpp>

namespace scwx
{
namespace qt
{
namespace gl
{
namespace draw
{

static const std::string logPrefix_ = "scwx::qt::gl::draw::placefile_text";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class PlacefileText::Impl
{
public:
   explicit Impl(const std::shared_ptr<GlContext>& context,
                 const std::string&                placefileName) :
       context_ {context}, placefileName_ {placefileName}
   {
   }

   ~Impl() {}

   void RenderTextDrawItem(
      const QMapLibreGL::CustomLayerRenderParameters&           params,
      const std::shared_ptr<const gr::Placefile::TextDrawItem>& di);
   void RenderText(const QMapLibreGL::CustomLayerRenderParameters& params,
                   const std::string&                              text,
                   const std::string&                              hoverText,
                   boost::gil::rgba8_pixel_t                       color,
                   float                                           x,
                   float                                           y);

   std::shared_ptr<GlContext> context_;

   std::string placefileName_;

   bool thresholded_ {false};

   std::chrono::system_clock::time_point selectedTime_ {};

   std::uint32_t textId_ {};
   glm::vec2     mapScreenCoordLocation_ {};
   float         mapScale_ {1.0f};
   float         mapBearingCos_ {1.0f};
   float         mapBearingSin_ {0.0f};
   float         halfWidth_ {};
   float         halfHeight_ {};
   std::string   hoverText_ {};

   units::length::nautical_miles<double> mapDistance_ {};

   std::mutex listMutex_ {};
   std::vector<std::shared_ptr<const gr::Placefile::TextDrawItem>> textList_ {};
   std::vector<std::shared_ptr<const gr::Placefile::TextDrawItem>> newList_ {};
};

PlacefileText::PlacefileText(const std::shared_ptr<GlContext>& context,
                             const std::string&                placefileName) :
    DrawItem(context->gl()), p(std::make_unique<Impl>(context, placefileName))
{
}
PlacefileText::~PlacefileText() = default;

PlacefileText::PlacefileText(PlacefileText&&) noexcept            = default;
PlacefileText& PlacefileText::operator=(PlacefileText&&) noexcept = default;

void PlacefileText::set_placefile_name(const std::string& placefileName)
{
   p->placefileName_ = placefileName;
}

void PlacefileText::set_selected_time(
   std::chrono::system_clock::time_point selectedTime)
{
   p->selectedTime_ = selectedTime;
}

void PlacefileText::set_thresholded(bool thresholded)
{
   p->thresholded_ = thresholded;
}

void PlacefileText::Initialize() {}

void PlacefileText::Render(
   const QMapLibreGL::CustomLayerRenderParameters& params)
{
   std::unique_lock lock {p->listMutex_};

   if (!p->textList_.empty())
   {
      // Reset text ID per frame
      p->textId_ = 0;
      p->hoverText_.clear();

      // Update map screen coordinate and scale information
      p->mapScreenCoordLocation_ = util::maplibre::LatLongToScreenCoordinate(
         {params.latitude, params.longitude});
      p->mapScale_ = std::pow(2.0, params.zoom) * mbgl::util::tileSize_D /
                     mbgl::util::DEGREES_MAX;
      p->mapBearingCos_ = cosf(params.bearing * common::kDegreesToRadians);
      p->mapBearingSin_ = sinf(params.bearing * common::kDegreesToRadians);
      p->halfWidth_     = params.width * 0.5f;
      p->halfHeight_    = params.height * 0.5f;
      p->mapDistance_   = util::maplibre::GetMapDistance(params);

      for (auto& di : p->textList_)
      {
         p->RenderTextDrawItem(params, di);
      }
   }
}

void PlacefileText::Impl::RenderTextDrawItem(
   const QMapLibreGL::CustomLayerRenderParameters&           params,
   const std::shared_ptr<const gr::Placefile::TextDrawItem>& di)
{
   // If no time has been selected, use the current time
   std::chrono::system_clock::time_point selectedTime =
      (selectedTime_ == std::chrono::system_clock::time_point {}) ?
         std::chrono::system_clock::now() :
         selectedTime_;

   if ((!thresholded_ || mapDistance_ <= di->threshold_) &&
       (di->startTime_ == std::chrono::system_clock::time_point {} ||
        (di->startTime_ <= selectedTime && selectedTime < di->endTime_)))
   {
      const auto screenCoordinates = (util::maplibre::LatLongToScreenCoordinate(
                                         {di->latitude_, di->longitude_}) -
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

      RenderText(params,
                 di->text_,
                 di->hoverText_,
                 di->color_,
                 rotatedX + di->x_ + halfWidth_,
                 rotatedY + di->y_ + halfHeight_);
   }
}

void PlacefileText::Impl::RenderText(
   const QMapLibreGL::CustomLayerRenderParameters& params,
   const std::string&                              text,
   const std::string&                              hoverText,
   boost::gil::rgba8_pixel_t                       color,
   float                                           x,
   float                                           y)
{
   const std::string windowName {
      fmt::format("PlacefileText-{}-{}", placefileName_, ++textId_)};

   // Convert screen to ImGui coordinates
   y = params.height - y;

   // Setup "window" to hold text
   ImGui::SetNextWindowPos(
      ImVec2 {x, y}, ImGuiCond_Always, ImVec2 {0.5f, 0.5f});
   ImGui::Begin(windowName.c_str(),
                nullptr,
                ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
                   ImGuiWindowFlags_NoBackground);

   // Render text
   ImGui::PushStyleColor(ImGuiCol_Text,
                         IM_COL32(color[0], color[1], color[2], color[3]));
   ImGui::TextUnformatted(text.c_str());
   ImGui::PopStyleColor();

   // Store hover text for mouse picking pass
   if (!hoverText.empty() && ImGui::IsItemHovered())
   {
      hoverText_ = hoverText;
   }

   // End window
   ImGui::End();
}

void PlacefileText::Deinitialize()
{
   std::unique_lock lock {p->listMutex_};

   // Clear the text list
   p->textList_.clear();
}

bool PlacefileText::RunMousePicking(
   const QMapLibreGL::CustomLayerRenderParameters& /* params */,
   const glm::vec2& /* mousePos */)
{
   bool itemPicked = false;

   // Create tooltip for hover text
   if (!p->hoverText_.empty())
   {
      itemPicked = true;
      util::ImGui::Instance().DrawTooltip(p->hoverText_);
   }

   return itemPicked;
}

void PlacefileText::StartText()
{
   // Clear the new list
   p->newList_.clear();
}

void PlacefileText::AddText(
   const std::shared_ptr<gr::Placefile::TextDrawItem>& di)
{
   if (di != nullptr)
   {
      p->newList_.emplace_back(di);
   }
}

void PlacefileText::FinishText()
{
   std::unique_lock lock {p->listMutex_};

   // Swap text lists
   p->textList_.swap(p->newList_);

   // Clear the new list
   p->newList_.clear();
}

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
