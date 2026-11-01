#include <scwx/qt/draw/placefile_polygons.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/qt/util/polygon_triangulation.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <scwx/qt/render/rhi_geo_colored_geometry_cache.hpp>
#include <scwx/qt/render/rhi_vulkan_overlay.hpp>

#include <mutex>

#include <boost/container/stable_vector.hpp>

namespace scwx
{
namespace qt
{
namespace draw
{

static const std::string logPrefix_ = "scwx::qt::draw::placefile_polygons";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr std::size_t kVerticesPerTriangle = 3;
// lat, long, x offset, y offset, r, g, b, a, perpendicular (unused)
static constexpr std::size_t kPointsPerVertex     = 9;
// Threshold, start time, end time
static constexpr std::size_t kIntegersPerVertex   = 3;

static constexpr std::size_t kTessVertexLat_ = 0;
static constexpr std::size_t kTessVertexLon_ = 1;
static constexpr std::size_t kTessVertexXOffset_ = 3;
static constexpr std::size_t kTessVertexYOffset_ = 4;
static constexpr std::size_t kTessVertexR_       = 5;
static constexpr std::size_t kTessVertexG_       = 6;
static constexpr std::size_t kTessVertexB_       = 7;
static constexpr std::size_t kTessVertexA_       = 8;
static constexpr std::size_t kTessVertexSize_    = kTessVertexA_ + 1;

using TessVertexArray = std::array<double, kTessVertexSize_>;

class PlacefilePolygons::Impl
{
public:
   explicit Impl(const std::shared_ptr<render::RenderContext>& context) :
       context_ {context}, numVertices_ {0}
   {
   }

   ~Impl() = default;

   void Update();

   void Tessellate(const std::shared_ptr<gr::Placefile::PolygonDrawItem>& di);

   void AppendVertex(const TessVertexArray& data);

   std::shared_ptr<render::RenderContext> context_;

   bool dirty_ {false};
   bool thresholded_ {false};

   std::chrono::system_clock::time_point selectedTime_ {};

   std::mutex                bufferMutex_ {};
   std::vector<float>        currentBuffer_ {};
   std::vector<std::int32_t> currentIntegerBuffer_ {};
   std::vector<float>        newBuffer_ {};
   std::vector<std::int32_t> newIntegerBuffer_ {};

   std::uint32_t numVertices_;

   render::RhiGeoColoredGeometryCache geoRenderer_ {};

   std::int32_t currentThreshold_ {};
   std::int32_t currentStartTime_ {};
   std::int32_t currentEndTime_ {};
};

PlacefilePolygons::PlacefilePolygons(
   const std::shared_ptr<render::RenderContext>& context) :
    DrawItem(), p(std::make_unique<Impl>(context))
{
}
PlacefilePolygons::~PlacefilePolygons() = default;

PlacefilePolygons::PlacefilePolygons(PlacefilePolygons&&) noexcept = default;
PlacefilePolygons&
PlacefilePolygons::operator=(PlacefilePolygons&&) noexcept = default;

void PlacefilePolygons::set_selected_time(
   std::chrono::system_clock::time_point selectedTime)
{ p->selectedTime_ = selectedTime; }

void PlacefilePolygons::set_thresholded(bool thresholded)
{ p->thresholded_ = thresholded; }

void PlacefilePolygons::Initialize() {}

void PlacefilePolygons::Render(
   const QMapLibre::CustomLayerRenderParameters& /* params */)
{
}

void PlacefilePolygons::RenderVulkan(
   QRhiCommandBuffer*                            commandBuffer,
   render::RhiVulkanOverlayResources&            resources,
   const QMapLibre::CustomLayerRenderParameters& params,
   bool /* textureAtlasChanged */)
{
   // Hold the buffer lock so a concurrent buffer swap cannot misalign the
   // vertex and integer buffers while they are uploaded.
   std::unique_lock lock {p->bufferMutex_};

   if (p->currentBuffer_.empty())
   {
      return;
   }

   p->Update();

   p->geoRenderer_.Render(commandBuffer,
                          resources,
                          params,
                          p->thresholded_,
                          p->selectedTime_,
                          p->currentBuffer_,
                          p->currentIntegerBuffer_,
                          p->numVertices_);
}

void PlacefilePolygons::Deinitialize()
{
   std::unique_lock lock {p->bufferMutex_};

   // Clear the current buffers
   p->currentBuffer_.clear();
   p->currentIntegerBuffer_.clear();
   p->geoRenderer_.Shutdown();
}

void PlacefilePolygons::StartPolygons()
{
   // Clear the new buffers
   p->newBuffer_.clear();
   p->newIntegerBuffer_.clear();
}

void PlacefilePolygons::AddPolygon(
   const std::shared_ptr<gr::Placefile::PolygonDrawItem>& di)
{
   if (di != nullptr)
   {
      p->Tessellate(di);
   }
}

void PlacefilePolygons::FinishPolygons()
{
   std::unique_lock lock {p->bufferMutex_};

   // Swap buffers
   p->currentBuffer_.swap(p->newBuffer_);
   p->currentIntegerBuffer_.swap(p->newIntegerBuffer_);

   // Release the swapped-out buffers and trim growth slack
   std::vector<float>().swap(p->newBuffer_);
   std::vector<std::int32_t>().swap(p->newIntegerBuffer_);
   p->currentBuffer_.shrink_to_fit();
   p->currentIntegerBuffer_.shrink_to_fit();

   // Mark the draw item dirty
   p->dirty_ = true;
   p->geoRenderer_.Invalidate();
}

void PlacefilePolygons::Impl::Update()
{
   // Caller must hold bufferMutex_
   if (dirty_)
   {
      numVertices_ =
         static_cast<std::uint32_t>(currentBuffer_.size() / kPointsPerVertex);

      dirty_ = false;
   }
}

void PlacefilePolygons::Impl::Tessellate(
   const std::shared_ptr<gr::Placefile::PolygonDrawItem>& di)
{
   boost::container::stable_vector<TessVertexArray> vertexAttributes {};
   std::vector<util::PolygonRing2D>                 polygon {};

   boost::gil::rgba8_pixel_t lastColor = di->color_;

   currentThreshold_ =
      static_cast<std::int32_t>(std::round(di->threshold_.value()));

   currentStartTime_ = static_cast<std::int32_t>(
      std::chrono::duration_cast<std::chrono::minutes>(
         di->startTime_.time_since_epoch())
         .count());
   currentEndTime_ = static_cast<std::int32_t>(
      std::chrono::duration_cast<std::chrono::minutes>(
         di->endTime_.time_since_epoch())
         .count());

   for (auto& contour : di->contours_)
   {
      util::PolygonRing2D ring {};
      ring.reserve(contour.size());

      for (auto& element : contour)
      {
         const auto screenCoordinate =
            util::maplibre::LatLongToScreenCoordinate(
               {element.latitude_, element.longitude_});

         if (element.color_.has_value())
         {
            lastColor = element.color_.value();
         }

         // Triangulate in projected space, but store lat/long for the GPU
         ring.push_back({screenCoordinate.x, screenCoordinate.y});
         vertexAttributes.emplace_back(TessVertexArray {element.latitude_,
                                                        element.longitude_,
                                                        0.0,
                                                        element.x_,
                                                        element.y_,
                                                        lastColor[0] / 255.0,
                                                        lastColor[1] / 255.0,
                                                        lastColor[2] / 255.0,
                                                        lastColor[3] / 255.0});
      }

      if (ring.size() >= 3)
      {
         polygon.push_back(std::move(ring));
      }
   }

   if (polygon.empty())
   {
      return;
   }

   const std::vector<std::uint32_t> indices = util::TriangulatePolygon(polygon);

   for (const std::uint32_t index : indices)
   {
      AppendVertex(vertexAttributes[index]);
   }

   while ((newBuffer_.size() / kPointsPerVertex) % kVerticesPerTriangle != 0)
   {
      newBuffer_.resize(newBuffer_.size() - kPointsPerVertex);
      newIntegerBuffer_.resize(newIntegerBuffer_.size() - kIntegersPerVertex);
   }
}

void PlacefilePolygons::Impl::AppendVertex(const TessVertexArray& data)
{
   newBuffer_.insert(newBuffer_.end(),
                     {static_cast<float>(data[kTessVertexLat_]),
                      static_cast<float>(data[kTessVertexLon_]),
                      static_cast<float>(data[kTessVertexXOffset_]),
                      static_cast<float>(data[kTessVertexYOffset_]),
                      static_cast<float>(data[kTessVertexR_]),
                      static_cast<float>(data[kTessVertexG_]),
                      static_cast<float>(data[kTessVertexB_]),
                      static_cast<float>(data[kTessVertexA_]),
                      0.0f});
   newIntegerBuffer_.insert(
      newIntegerBuffer_.end(),
      {currentThreshold_, currentStartTime_, currentEndTime_});
}

} // namespace draw
} // namespace qt
} // namespace scwx
