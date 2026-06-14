#include <scwx/qt/gl/draw/placefile_triangles.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#if defined(SCWX_RENDER_BACKEND_VULKAN)
#   include <scwx/qt/render/projection.hpp>
#   include <scwx/qt/render/rhi_colored_geometry.hpp>
#   include <scwx/qt/render/rhi_vulkan_overlay.hpp>
#endif

#include <mutex>

namespace scwx
{
namespace qt
{
namespace gl
{
namespace draw
{

static const std::string logPrefix_ = "scwx::qt::gl::draw::placefile_triangles";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr std::size_t kVerticesPerTriangle = 3;
static constexpr std::size_t kPointsPerVertex     = 8;

// Threshold, start time, end time
static constexpr std::size_t kIntegersPerVertex_ = 3;

class PlacefileTriangles::Impl
{
public:
   explicit Impl(const std::shared_ptr<render::RenderContext>& context) :
       context_ {context},
       numVertices_ {0}
   {
   }

   ~Impl() {}

   void UpdateBuffers(
      const std::shared_ptr<const gr::Placefile::TrianglesDrawItem>& di);
   void Update();

   std::shared_ptr<render::RenderContext> context_;

   bool dirty_ {false};
   bool thresholded_ {false};

   std::chrono::system_clock::time_point selectedTime_ {};

   std::mutex bufferMutex_ {};

   std::vector<float>        currentBuffer_ {};
   std::vector<std::int32_t> currentIntegerBuffer_ {};
   std::vector<float>        newBuffer_ {};
   std::vector<std::int32_t> newIntegerBuffer_ {};

   std::uint32_t numVertices_;
};

PlacefileTriangles::PlacefileTriangles(
   const std::shared_ptr<render::RenderContext>& context) :
    DrawItem(), p(std::make_unique<Impl>(context))
{
}
PlacefileTriangles::~PlacefileTriangles() = default;

PlacefileTriangles::PlacefileTriangles(PlacefileTriangles&&) noexcept = default;
PlacefileTriangles&
PlacefileTriangles::operator=(PlacefileTriangles&&) noexcept = default;

void PlacefileTriangles::set_selected_time(
   std::chrono::system_clock::time_point selectedTime)
{
   p->selectedTime_ = selectedTime;
}

void PlacefileTriangles::set_thresholded(bool thresholded)
{
   p->thresholded_ = thresholded;
}

void PlacefileTriangles::Initialize()
{
}

void PlacefileTriangles::Render(
   const QMapLibre::CustomLayerRenderParameters& /* params */)
{
}

#if defined(SCWX_RENDER_BACKEND_VULKAN)
void PlacefileTriangles::RenderVulkan(
   QRhiCommandBuffer*                            commandBuffer,
   render::RhiVulkanOverlayResources&            resources,
   const QMapLibre::CustomLayerRenderParameters& params,
   bool /* textureAtlasChanged */)
{
   if (p->currentBuffer_.empty())
   {
      return;
   }

   p->Update();

   std::vector<std::int32_t> integerVertices(p->currentIntegerBuffer_.begin(),
                                               p->currentIntegerBuffer_.end());
   const std::vector<float> transformedVertices =
      render::TransformMapColorVertices(p->currentBuffer_,
                                          integerVertices,
                                          params,
                                          p->thresholded_,
                                          p->selectedTime_);

   if (transformedVertices.empty())
   {
      return;
   }

   const glm::mat4 identity {1.0f};
   resources.coloredGeometry.Render(
      commandBuffer,
      identity,
      transformedVertices,
      transformedVertices.size() / 7);
}
#endif

void PlacefileTriangles::Deinitialize()
{
   std::unique_lock lock {p->bufferMutex_};

   // Clear the current buffers
   p->currentBuffer_.clear();
   p->currentIntegerBuffer_.clear();
}

void PlacefileTriangles::StartTriangles()
{
   // Clear the new buffers
   p->newBuffer_.clear();
   p->newIntegerBuffer_.clear();
}

void PlacefileTriangles::AddTriangles(
   const std::shared_ptr<gr::Placefile::TrianglesDrawItem>& di)
{
   if (di != nullptr)
   {
      p->UpdateBuffers(di);
   }
}

void PlacefileTriangles::FinishTriangles()
{
   std::unique_lock lock {p->bufferMutex_};

   // Swap buffers
   p->currentBuffer_.swap(p->newBuffer_);
   p->currentIntegerBuffer_.swap(p->newIntegerBuffer_);

   // Clear the new buffers
   p->newBuffer_.clear();
   p->newIntegerBuffer_.clear();

   // Mark the draw item dirty
   p->dirty_ = true;
}

void PlacefileTriangles::Impl::UpdateBuffers(
   const std::shared_ptr<const gr::Placefile::TrianglesDrawItem>& di)
{
   // Threshold value
   units::length::nautical_miles<double> threshold = di->threshold_;
   auto thresholdValue =
      static_cast<std::int32_t>(std::round(threshold.value()));

   // Start and end time
   auto startTime =
      static_cast<std::int32_t>(std::chrono::duration_cast<std::chrono::minutes>(
                            di->startTime_.time_since_epoch())
                            .count());
   auto endTime =
      static_cast<std::int32_t>(std::chrono::duration_cast<std::chrono::minutes>(
                            di->endTime_.time_since_epoch())
                            .count());

   // Default color to "Color" statement
   boost::gil::rgba8_pixel_t lastColor = di->color_;

   // For each element inside a Triangles statement, add a vertex
   for (auto& element : di->elements_)
   {
      // Calculate screen coordinate
      auto screenCoordinate = util::maplibre::LatLongToScreenCoordinate(
         {element.latitude_, element.longitude_});

      // X/Y offset in pixels
      const float x = static_cast<float>(element.x_);
      const float y = static_cast<float>(element.y_);

      // Update the most recent color if specified
      if (element.color_.has_value())
      {
         lastColor = element.color_.value();
      }

      // Color value
      const float r = lastColor[0] / 255.0f;
      const float g = lastColor[1] / 255.0f;
      const float b = lastColor[2] / 255.0f;
      const float a = lastColor[3] / 255.0f;

      newBuffer_.insert(
         newBuffer_.end(),
         {screenCoordinate.x, screenCoordinate.y, x, y, r, g, b, a});
      newIntegerBuffer_.insert(newIntegerBuffer_.end(),
                               {thresholdValue, startTime, endTime});
   }

   // Remove extra vertices that don't correspond to a full triangle
   while (newBuffer_.size() % kVerticesPerTriangle != 0)
   {
      newBuffer_.pop_back();
      newIntegerBuffer_.pop_back();
   }
}

void PlacefileTriangles::Impl::Update()
{
   if (dirty_)
   {
      std::unique_lock lock {bufferMutex_};

      numVertices_ =
         static_cast<std::uint32_t>(currentBuffer_.size() / kPointsPerVertex);

      dirty_ = false;
   }
}

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
