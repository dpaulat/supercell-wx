#include <scwx/qt/map/placefile_layer.hpp>
#include <scwx/util/logger.hpp>

#include <fmt/format.h>
#include <imgui.h>

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
   explicit Impl(std::shared_ptr<MapContext> context) {};
   ~Impl() = default;

   void RenderText(const std::string&        text,
                   const std::string&        hoverText,
                   boost::gil::rgba8_pixel_t color,
                   float                     x,
                   float                     y);

   std::uint32_t textId_ {};
};

PlacefileLayer::PlacefileLayer(std::shared_ptr<MapContext> context) :
    DrawLayer(context), p(std::make_unique<PlacefileLayer::Impl>(context))
{
}

PlacefileLayer::~PlacefileLayer() = default;

void PlacefileLayer::Initialize()
{
   logger_->debug("Initialize()");

   DrawLayer::Initialize();
}

void PlacefileLayer::Impl::RenderText(const std::string&        text,
                                      const std::string&        hoverText,
                                      boost::gil::rgba8_pixel_t color,
                                      float                     x,
                                      float                     y)
{
   const std::string windowName {fmt::format("PlacefileText-{}", ++textId_)};

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
      ImGui::TextUnformatted(hoverText.c_str());
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

   // Render text

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
